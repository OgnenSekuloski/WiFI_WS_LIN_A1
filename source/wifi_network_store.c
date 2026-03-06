/*******************************************************************************
* File Name: wifi_network_store.c
*
* Description:
*   Persistent Wi-Fi network storage in INTERNAL flash (PSoC 6).
*
*   What this module does:
*     - Stores up to WIFI_STORE_MAX_NETWORKS Wi-Fi credentials (SSID + password)
*       in internal flash.
*     - Uses a simple “one record per flash page” layout.
*     - Each record includes a monotonically increasing counter used as
*       “last used” (LRU metadata).
*     - When saving and the store is full, the least-recently-used record is
*       overwritten (evicted).
*     - Uses CRC32 to detect corruption / partially programmed pages.
*
*   Where it stores data:
*     - At the address provided by the linker symbol __wifi_creds_flash_start
*       (typically placed into the em_eeprom memory region in linker.ld).
*
* IMPORTANT (your current linker error):
*   If you see: undefined reference to `__wifi_creds_flash_start`
*   then your linker script did NOT export the symbol correctly.
*
*   In linker.ld, DO THIS (recommended):
*     PROVIDE(__wifi_creds_flash_start = ORIGIN(em_eeprom));
*
*   (Do not rely on a plain assignment; PROVIDE is safer and conventional.)
*
*******************************************************************************/

#include "wifi_network_store.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cyhal_flash.h"

/* If you use FreeRTOS in the project, we protect flash ops with a critical section. */
#include "FreeRTOS.h"
#include "task.h"

/*------------------------------------------------------------------------------
 * Linker symbol exported from linker.ld (address where store starts)
 *----------------------------------------------------------------------------*/
extern uint32_t __wifi_creds_flash_start;

/*------------------------------------------------------------------------------
 * Record format (one record per flash page)
 *----------------------------------------------------------------------------*/
#define WIFI_STORE_MAGIC        (0x4E455457u) /* "NETW" in ASCII */
#define WIFI_STORE_VERSION      (1u)

/* We keep a conservative cap for page buffer size. Many CAT1 parts use 512B/1024B
 * pages; 4096 keeps us safe unless your flash uses larger pages. */
#define WIFI_STORE_MAX_PAGE_SIZE (4096u)

/* One record per flash page (packed so fields are laid out predictably). */
typedef struct __attribute__((packed))
{
    uint32_t magic;             /* Must be WIFI_STORE_MAGIC */
    uint16_t version;           /* Must be WIFI_STORE_VERSION */
    uint16_t reserved;          /* Padding / future use */

    uint32_t last_used_counter; /* Higher value = more recently used */
    uint32_t crc32;             /* CRC32 over (last_used_counter + ssid + pwd) */

    char ssid[WIFI_SSID_MAX_LEN + 1];
    char pwd[WIFI_PWD_MAX_LEN + 1];
} wifi_store_record_t;

/*------------------------------------------------------------------------------
 * HAL objects / module state
 *----------------------------------------------------------------------------*/
static cyhal_flash_t s_flash_obj;
static bool s_inited = false;

/* Page buffer required by cyhal_flash_write(). Must be 4-byte aligned. */
static uint8_t s_page_buf[WIFI_STORE_MAX_PAGE_SIZE] __attribute__((aligned(4)));

static cy_rslt_t write_record_to_slot(uint32_t addr,
                                      uint32_t page_size,
                                      uint32_t sector_size,
                                      uint8_t erase_value,
                                      const wifi_store_record_t* rec);

/*------------------------------------------------------------------------------
 * Small CRC32 implementation (polynomial 0xEDB88320)
 * NOTE: This is not crypto; it is only for integrity (detect corruption).
 *----------------------------------------------------------------------------*/
static uint32_t crc32_compute(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            uint32_t mask = (uint32_t)(-(int)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

static uint32_t record_crc32_calc(const wifi_store_record_t* rec)
{
    /* CRC is computed over:
     *   last_used_counter (4 bytes)
     *   ssid buffer        (WIFI_SSID_MAX_LEN+1 bytes)
     *   pwd buffer         (WIFI_PWD_MAX_LEN+1 bytes)
     *
     * We do it in 3 segments to avoid relying on struct packing beyond fields.
     */
    uint32_t c = 0xFFFFFFFFu;

    /* Segment 1: last_used_counter */
    {
        uint32_t v = rec->last_used_counter;
        c = crc32_compute((const uint8_t*)&v, sizeof(v));
    }

    /* Segment 2: ssid */
    {
        uint32_t c2 = crc32_compute((const uint8_t*)rec->ssid, (WIFI_SSID_MAX_LEN + 1));
        c ^= c2;
    }

    /* Segment 3: pwd */
    {
        uint32_t c3 = crc32_compute((const uint8_t*)rec->pwd, (WIFI_PWD_MAX_LEN + 1));
        c ^= c3;
    }

    return c;
}

/*------------------------------------------------------------------------------
 * Initialize flash HAL
 *----------------------------------------------------------------------------*/
cy_rslt_t wifi_store_init(void)
{
    if (s_inited)
    {
        return CY_RSLT_SUCCESS;
    }

    cy_rslt_t r = cyhal_flash_init(&s_flash_obj);
    if (r == CY_RSLT_SUCCESS)
    {
        s_inited = true;
    }

    return r;
}

/*------------------------------------------------------------------------------
 * Helpers: flash geometry for the chosen address
 *
 * cyhal_flash_get_info() returns an info struct with one or more “blocks”.
 * Each block describes a region of flash and its:
 *   - start_address
 *   - size
 *   - sector_size   (erase granularity)
 *   - page_size     (program granularity)
 *   - erase_value   (usually 0xFF)
 *
 * We locate which block contains our base address.
 *----------------------------------------------------------------------------*/
static bool find_block_for_addr(uint32_t addr,
                                const cyhal_flash_info_t* info,
                                cyhal_flash_block_info_t* out_block)
{
    if (info == NULL || out_block == NULL)
        return false;

    if (info->block_count == 0 || info->blocks == NULL)
        return false;

    for (uint8_t i = 0; i < info->block_count; i++)
    {
        const cyhal_flash_block_info_t* b = &info->blocks[i];
        uint32_t start = b->start_address;
        uint32_t end   = start + b->size;

        if ((addr >= start) && (addr < end))
        {
            *out_block = *b;
            return true;
        }
    }

    return false;
}

static cy_rslt_t get_store_geometry(uint32_t* out_base_addr,
                                   uint32_t* out_page_size,
                                   uint32_t* out_sector_size,
                                   uint8_t* out_erase_value)
{
    if (!out_base_addr || !out_page_size || !out_sector_size || !out_erase_value)
        return CY_RSLT_TYPE_ERROR;

    if (!s_inited)
        return CY_RSLT_TYPE_ERROR;

    /* Our base address is whatever the linker provided. */
    uint32_t base = (uint32_t)&__wifi_creds_flash_start;

    cyhal_flash_info_t info;
    cyhal_flash_get_info(&s_flash_obj, &info);

    cyhal_flash_block_info_t block;
    if (!find_block_for_addr(base, &info, &block))
        return CY_RSLT_TYPE_ERROR;

    if (block.page_size == 0 || block.page_size > WIFI_STORE_MAX_PAGE_SIZE)
        return CY_RSLT_TYPE_ERROR;

    if (block.sector_size == 0)
        return CY_RSLT_TYPE_ERROR;

    /* We require the store base to be aligned to a flash page boundary because
     * we write whole pages for each record. */
    if ((base % block.page_size) != 0u)
        return CY_RSLT_TYPE_ERROR;

    *out_base_addr   = base;
    *out_page_size   = block.page_size;
    *out_sector_size = block.sector_size;
    *out_erase_value = block.erase_value;

    return CY_RSLT_SUCCESS;
}

/*------------------------------------------------------------------------------
 * Slot addressing (one record per page)
 *----------------------------------------------------------------------------*/
static uint32_t slot_addr(uint32_t base, uint32_t page_size, uint32_t slot_index)
{
    return base + (slot_index * page_size);
}

/*------------------------------------------------------------------------------
 * Record validity check (magic + version + CRC + non-empty SSID)
 *----------------------------------------------------------------------------*/
static bool record_is_valid(const wifi_store_record_t* rec)
{
    if (rec == NULL)
        return false;

    if (rec->magic != WIFI_STORE_MAGIC)
        return false;

    if (rec->version != WIFI_STORE_VERSION)
        return false;

    if (rec->ssid[0] == '\0')
        return false;

    /* Verify CRC */
    return (record_crc32_calc(rec) == rec->crc32);
}

/*------------------------------------------------------------------------------
 * Read all valid networks and return them ordered by most recent
 *----------------------------------------------------------------------------*/
cy_rslt_t wifi_store_get_known_networks(wifi_network_t* out_list,
                                        uint32_t max_list,
                                        uint32_t* out_count)
{
    if (out_list == NULL || out_count == NULL)
        return CY_RSLT_TYPE_ERROR;

    *out_count = 0;

    cy_rslt_t r = wifi_store_init();
    if (r != CY_RSLT_SUCCESS)
        return r;

    uint32_t base = 0, page_size = 0, sector_size = 0;
    uint8_t erase_value = 0xFF;
    r = get_store_geometry(&base, &page_size, &sector_size, &erase_value);
    if (r != CY_RSLT_SUCCESS)
        return r;

    /* Gather valid records */
    wifi_network_t nets[WIFI_STORE_MAX_NETWORKS];
    uint32_t counters[WIFI_STORE_MAX_NETWORKS];
    uint32_t found = 0;

    for (uint32_t i = 0; i < WIFI_STORE_MAX_NETWORKS; i++)
    {
        const wifi_store_record_t* rec =
            (const wifi_store_record_t*)slot_addr(base, page_size, i);

        if (record_is_valid(rec))
        {
            counters[found] = rec->last_used_counter;

            memset(&nets[found], 0, sizeof(nets[found]));
            strncpy(nets[found].ssid, rec->ssid, WIFI_SSID_MAX_LEN);
            nets[found].ssid[WIFI_SSID_MAX_LEN] = '\0';

            strncpy(nets[found].pwd, rec->pwd, WIFI_PWD_MAX_LEN);
            nets[found].pwd[WIFI_PWD_MAX_LEN] = '\0';

            found++;
            if (found >= WIFI_STORE_MAX_NETWORKS)
                break;
        }
    }

    /* Sort by counter DESC (most recent first).
     * Bubble sort is fine for small N (<=8). */
    for (uint32_t a = 0; a < found; a++)
    {
        for (uint32_t b = a + 1; b < found; b++)
        {
            if (counters[b] > counters[a])
            {
                uint32_t tc = counters[a];
                counters[a] = counters[b];
                counters[b] = tc;

                wifi_network_t tn = nets[a];
                nets[a] = nets[b];
                nets[b] = tn;
            }
        }
    }

    uint32_t to_copy = (found < max_list) ? found : max_list;
    for (uint32_t i = 0; i < to_copy; i++)
    {
        out_list[i] = nets[i];
    }

    *out_count = to_copy;
    return CY_RSLT_SUCCESS;
}

/*------------------------------------------------------------------------------
 * Helper: find the next counter value (max + 1)
 *----------------------------------------------------------------------------*/
static uint32_t compute_next_counter(uint32_t base, uint32_t page_size)
{
    uint32_t maxc = 0;

    for (uint32_t i = 0; i < WIFI_STORE_MAX_NETWORKS; i++)
    {
        const wifi_store_record_t* rec =
            (const wifi_store_record_t*)slot_addr(base, page_size, i);

        if (record_is_valid(rec) && rec->last_used_counter > maxc)
        {
            maxc = rec->last_used_counter;
        }
    }

    return maxc + 1u;
}

/*------------------------------------------------------------------------------
 * Deterministic overflow recovery:
 *   - Read all valid records
 *   - Sort by old counter ascending (oldest -> newest), with slot index tie-break
 *   - Rewrite counters to 1..N
 *----------------------------------------------------------------------------*/
static cy_rslt_t renormalize_counters(uint32_t base,
                                      uint32_t page_size,
                                      uint32_t sector_size,
                                      uint8_t erase_value)
{
    typedef struct
    {
        uint32_t slot;
        uint32_t counter;
        wifi_store_record_t rec;
    } ranked_record_t;

    ranked_record_t records[WIFI_STORE_MAX_NETWORKS];
    uint32_t count = 0;

    for (uint32_t i = 0; i < WIFI_STORE_MAX_NETWORKS; i++)
    {
        const wifi_store_record_t* rec =
            (const wifi_store_record_t*)slot_addr(base, page_size, i);

        if (!record_is_valid(rec))
        {
            continue;
        }

        records[count].slot = i;
        records[count].counter = rec->last_used_counter;
        records[count].rec = *rec;
        count++;
    }

    if (count == 0u)
    {
        return CY_RSLT_SUCCESS;
    }

    for (uint32_t a = 0; a < count; a++)
    {
        for (uint32_t b = a + 1; b < count; b++)
        {
            bool should_swap = false;

            if (records[b].counter < records[a].counter)
            {
                should_swap = true;
            }
            else if ((records[b].counter == records[a].counter) &&
                     (records[b].slot < records[a].slot))
            {
                should_swap = true;
            }

            if (should_swap)
            {
                ranked_record_t t = records[a];
                records[a] = records[b];
                records[b] = t;
            }
        }
    }

    for (uint32_t i = 0; i < count; i++)
    {
        records[i].rec.last_used_counter = i + 1u;
        records[i].rec.crc32 = record_crc32_calc(&records[i].rec);

        uint32_t addr = slot_addr(base, page_size, records[i].slot);
        cy_rslt_t r = write_record_to_slot(addr, page_size, sector_size, erase_value, &records[i].rec);
        if (r != CY_RSLT_SUCCESS)
        {
            return r;
        }
    }

    return CY_RSLT_SUCCESS;
}

/*------------------------------------------------------------------------------
 * Helper: locate
 *   - matching SSID slot (if exists)
 *   - first free/invalid slot (if exists)
 *   - LRU slot (lowest counter among valid records)
 *----------------------------------------------------------------------------*/
static void find_slots(uint32_t base, uint32_t page_size,
                       const char* ssid,
                       int* out_match,
                       int* out_free,
                       int* out_lru,
                       uint32_t* out_lru_counter)
{
    *out_match = -1;
    *out_free  = -1;
    *out_lru   = -1;
    *out_lru_counter = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < WIFI_STORE_MAX_NETWORKS; i++)
    {
        const wifi_store_record_t* rec =
            (const wifi_store_record_t*)slot_addr(base, page_size, i);

        if (record_is_valid(rec))
        {
            if (ssid != NULL && (strncmp(rec->ssid, ssid, WIFI_SSID_MAX_LEN) == 0))
            {
                *out_match = (int)i;
            }

            if (rec->last_used_counter < *out_lru_counter)
            {
                *out_lru_counter = rec->last_used_counter;
                *out_lru = (int)i;
            }
        }
        else
        {
            if (*out_free < 0)
            {
                *out_free = (int)i;
            }
        }
    }
}

/*------------------------------------------------------------------------------
 * Flash write helper:
 *   - Erase the sector that contains the page address
 *   - Program one full page
 *
 * Why erase?
 *   Flash can only change bits from 1 -> 0 when programming.
 *   To go back to 1, you must erase a whole sector (sets bytes to erase_value,
 *   typically 0xFF). Therefore, every time we overwrite a page, we erase the
 *   containing sector first.
 *
 * NOTE:
 *   This approach is simplest and works well for small N (<=8 networks) and
 *   infrequent writes (only when provisioning or marking used).
 *----------------------------------------------------------------------------*/
static cy_rslt_t write_record_to_slot(uint32_t addr,
                                      uint32_t page_size,
                                      uint32_t sector_size,
                                      uint8_t erase_value,
                                      const wifi_store_record_t* rec)
{
    /* Prepare a full-page buffer */
    memset(s_page_buf, erase_value, page_size);
    memcpy(s_page_buf, rec, sizeof(*rec));

    /* Compute sector base (align address down to sector boundary) */
    uint32_t sector_base = addr - (addr % sector_size);

    /* Flash operations must not be interrupted by another flash op.
     * Critical section is a simple way to avoid trouble. */
    taskENTER_CRITICAL();

    /* Erase the sector that contains this page */
    cy_rslt_t r = cyhal_flash_erase(&s_flash_obj, sector_base);

    /* Program the page (HAL expects uint32_t pointer) */
    if (r == CY_RSLT_SUCCESS)
    {
        r = cyhal_flash_write(&s_flash_obj, addr, (const uint32_t*)s_page_buf);
    }

    taskEXIT_CRITICAL();

    return r;
}

/*------------------------------------------------------------------------------
 * Save/update network (with eviction)
 *----------------------------------------------------------------------------*/
cy_rslt_t wifi_store_save_network(const char* ssid, const char* pwd)
{
    if (ssid == NULL || ssid[0] == '\0')
        return CY_RSLT_TYPE_ERROR;

    cy_rslt_t r = wifi_store_init();
    if (r != CY_RSLT_SUCCESS)
        return r;

    uint32_t base = 0, page_size = 0, sector_size = 0;
    uint8_t erase_value = 0xFF;
    r = get_store_geometry(&base, &page_size, &sector_size, &erase_value);
    if (r != CY_RSLT_SUCCESS)
        return r;

    int match = -1, free_slot = -1, lru = -1;
    uint32_t lru_counter = 0xFFFFFFFFu;

    find_slots(base, page_size, ssid, &match, &free_slot, &lru, &lru_counter);

    /* Choose target slot:
     *   1) overwrite matching SSID, else
     *   2) use free slot, else
     *   3) evict LRU slot
     */
    int target = (match >= 0) ? match : ((free_slot >= 0) ? free_slot : lru);
    if (target < 0)
        return CY_RSLT_TYPE_ERROR;

    wifi_store_record_t rec;
    memset(&rec, 0, sizeof(rec));

    rec.magic = WIFI_STORE_MAGIC;
    rec.version = WIFI_STORE_VERSION;

    uint32_t next_counter = compute_next_counter(base, page_size);
    if (next_counter == 0u)
    {
        r = renormalize_counters(base, page_size, sector_size, erase_value);
        if (r != CY_RSLT_SUCCESS)
        {
            return r;
        }

        next_counter = compute_next_counter(base, page_size);
        if (next_counter == 0u)
        {
            return CY_RSLT_TYPE_ERROR;
        }
    }
    rec.last_used_counter = next_counter;

    strncpy(rec.ssid, ssid, WIFI_SSID_MAX_LEN);
    rec.ssid[WIFI_SSID_MAX_LEN] = '\0';

    if (pwd != NULL)
    {
        strncpy(rec.pwd, pwd, WIFI_PWD_MAX_LEN);
        rec.pwd[WIFI_PWD_MAX_LEN] = '\0';
    }
    else
    {
        rec.pwd[0] = '\0';
    }

    rec.crc32 = record_crc32_calc(&rec);

    uint32_t addr = slot_addr(base, page_size, (uint32_t)target);
    return write_record_to_slot(addr, page_size, sector_size, erase_value, &rec);
}

/*------------------------------------------------------------------------------
 * Mark SSID as recently used:
 *   - Find existing record
 *   - Re-save it (same SSID/pwd) which bumps counter
 *
 * This is what gives you the “delete the oldest when full” behavior.
 *----------------------------------------------------------------------------*/
cy_rslt_t wifi_store_mark_used(const char* ssid)
{
    if (ssid == NULL || ssid[0] == '\0')
        return CY_RSLT_TYPE_ERROR;

    cy_rslt_t r = wifi_store_init();
    if (r != CY_RSLT_SUCCESS)
        return r;

    uint32_t base = 0, page_size = 0, sector_size = 0;
    uint8_t erase_value = 0xFF;
    r = get_store_geometry(&base, &page_size, &sector_size, &erase_value);
    if (r != CY_RSLT_SUCCESS)
        return r;

    int match = -1, free_slot = -1, lru = -1;
    uint32_t lru_counter = 0xFFFFFFFFu;

    find_slots(base, page_size, ssid, &match, &free_slot, &lru, &lru_counter);
    if (match < 0)
    {
        /* Not found -> nothing to do */
        return CY_RSLT_SUCCESS;
    }

    const wifi_store_record_t* oldrec =
        (const wifi_store_record_t*)slot_addr(base, page_size, (uint32_t)match);

    if (!record_is_valid(oldrec))
    {
        /* Slot is not valid -> nothing to do */
        return CY_RSLT_SUCCESS;
    }

    /* Re-save to bump last_used_counter */
    return wifi_store_save_network(oldrec->ssid, oldrec->pwd);
}
