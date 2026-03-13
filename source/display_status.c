/*******************************************************************************
* File Name:   display_status.c
*
* Description: This file contains the display helper implementation used to
*              render Wi-Fi provisioning/connection progress and LIN actuator
*              runtime status on the attached OLED module.
*
*******************************************************************************/

#include "display_status.h"

#ifdef ENABLE_OLED

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cyhal.h"
#include "cybsp.h"

#define OLED_I2C_ADDRESS               (0x3Cu)
#define OLED_I2C_FREQUENCY_HZ          (400000u)
#define OLED_I2C_TIMEOUT_MS            (10u)

#define OLED_WIDTH                     (128u)
#define OLED_HEIGHT                    (64u)
#define OLED_PAGE_COUNT                (OLED_HEIGHT / 8u)
#define OLED_BUFFER_SIZE               (OLED_WIDTH * OLED_PAGE_COUNT)
#define OLED_CHAR_WIDTH                (6u)
#define OLED_TEXT_COLS                 (OLED_WIDTH / OLED_CHAR_WIDTH)
#define OLED_TEXT_ROWS                 (OLED_PAGE_COUNT)
#define OLED_STATUS_START_ROW          (4u)

static cyhal_i2c_t oled_i2c;
static bool oled_ready = false;
static uint8_t oled_buffer[OLED_BUFFER_SIZE];

static cy_rslt_t oled_flush(void);

/*******************************************************************************
* Function Name: oled_reverse_bits
********************************************************************************
* Summary:
*  Reverses the bit order of a byte. Used to correct numeric glyph orientation
*  on the OLED without changing the orientation of all other glyphs.
*******************************************************************************/
static uint8_t oled_reverse_bits(uint8_t value)
{
    value = (uint8_t)(((value & 0xF0u) >> 4) | ((value & 0x0Fu) << 4));
    value = (uint8_t)(((value & 0xCCu) >> 2) | ((value & 0x33u) << 2));
    value = (uint8_t)(((value & 0xAAu) >> 1) | ((value & 0x55u) << 1));
    return value;
}

/*******************************************************************************
* Function Name: oled_prepare_status_text
********************************************************************************
* Summary:
*  Converts the HTML-oriented LIN status text into compact plain text for the
*  OLED. This strips `<br>` tags and shortens calibration wording so it fits on
*  the small display.
*******************************************************************************/
static void oled_prepare_status_text(char *dst, size_t dst_size, const char *src)
{
    char state[64] = {0};
    char learned[16] = {0};
    char last_event[128] = {0};
    const char *state_tag = "LIN State: ";
    const char *learned_tag = "Learned Positions: ";
    const char *event_tag = "Last Event: ";
    const char *state_start;
    const char *learned_start;
    const char *event_start;
    const char *state_end;
    const char *learned_end;
    size_t state_len;
    size_t learned_len;
    size_t event_len;

    if ((dst == NULL) || (dst_size == 0u))
    {
        return;
    }

    dst[0] = '\0';

    if (src == NULL)
    {
        return;
    }

    state_start = strstr(src, state_tag);
    learned_start = strstr(src, learned_tag);
    event_start = strstr(src, event_tag);

    if ((state_start == NULL) || (learned_start == NULL))
    {
        snprintf(dst, dst_size, "%s", src);
        return;
    }

    state_start += strlen(state_tag);
    learned_start += strlen(learned_tag);
    state_end = strstr(state_start, "<br>");
    learned_end = strstr(learned_start, "<br>");

    if (state_end == NULL)
    {
        state_end = learned_start - strlen(learned_tag);
    }

    if (learned_end == NULL)
    {
        learned_end = src + strlen(src);
    }

    state_len = (size_t)(state_end - state_start);
    if (state_len >= sizeof(state))
    {
        state_len = sizeof(state) - 1u;
    }
    memcpy(state, state_start, state_len);
    state[state_len] = '\0';

    learned_len = (size_t)(learned_end - learned_start);
    if (learned_len >= sizeof(learned))
    {
        learned_len = sizeof(learned) - 1u;
    }
    memcpy(learned, learned_start, learned_len);
    learned[learned_len] = '\0';

    if (event_start != NULL)
    {
        event_start += strlen(event_tag);
        event_len = strlen(event_start);
        if (event_len >= sizeof(last_event))
        {
            event_len = sizeof(last_event) - 1u;
        }
        memcpy(last_event, event_start, event_len);
        last_event[event_len] = '\0';
    }

    if (strncmp(last_event, "Calibration", strlen("Calibration")) == 0)
    {
        memmove(last_event + strlen("CALIB"),
                last_event + strlen("Calibration"),
                strlen(last_event + strlen("Calibration")) + 1u);
        memcpy(last_event, "CALIB", strlen("CALIB"));
    }
    else
    {
        char *calib = strstr(last_event, "CALIBRATION");
        if (calib != NULL)
        {
            memmove(calib + strlen("CALIB"),
                    calib + strlen("CALIBRATION"),
                    strlen(calib + strlen("CALIBRATION")) + 1u);
            memcpy(calib, "CALIB", strlen("CALIB"));
        }
    }

    if ((strncmp(last_event, "Calibration finished.", strlen("Calibration finished.")) == 0) ||
        (strncmp(last_event, "CALIB finished.", strlen("CALIB finished.")) == 0))
    {
        snprintf(last_event, sizeof(last_event), "CALIBRATION FINISHED");
    }
    else if (strncmp(last_event, "Open command accepted.", strlen("Open command accepted.")) == 0)
    {
        snprintf(last_event, sizeof(last_event), "OPEN");
    }
    else if (strncmp(last_event, "Close command accepted.", strlen("Close command accepted.")) == 0)
    {
        snprintf(last_event, sizeof(last_event), "CLOSE");
    }

    if (strcmp(last_event, "LIN actuator module initialized.") == 0)
    {
        snprintf(dst,
                 dst_size,
                 "LIN State: %s\nLearned Pos.: %s",
                 state,
                 learned);
    }
    else if (last_event[0] != '\0')
    {
        snprintf(dst,
                 dst_size,
                 "LIN State: %s\nLearned Pos.: %s\nLast Event: %s",
                 state,
                 learned,
                 last_event);
    }
    else
    {
        snprintf(dst,
                 dst_size,
                 "LIN State: %s\nLearned Pos.: %s",
                 state,
                 learned);
    }
}

/*******************************************************************************
* Function Name: oled_write
********************************************************************************
* Summary:
*  Writes a command or data stream to the OLED over I2C.
*******************************************************************************/
static cy_rslt_t oled_write(uint8_t control, const uint8_t *data, size_t length)
{
    uint8_t tx[17];
    size_t offset = 0u;

    tx[0] = control;

    while (offset < length)
    {
        size_t chunk = length - offset;
        if (chunk > (sizeof(tx) - 1u))
        {
            chunk = sizeof(tx) - 1u;
        }

        memcpy(&tx[1], &data[offset], chunk);
        cy_rslt_t result = cyhal_i2c_master_write(&oled_i2c, OLED_I2C_ADDRESS,
                                                  tx, (uint16_t)(chunk + 1u),
                                                  OLED_I2C_TIMEOUT_MS, true);
        if (result != CY_RSLT_SUCCESS)
        {
            return result;
        }

        offset += chunk;
    }

    return CY_RSLT_SUCCESS;
}

/*******************************************************************************
* Function Name: oled_write_commands
********************************************************************************
* Summary:
*  Writes a consecutive set of controller commands to the OLED.
*******************************************************************************/
static cy_rslt_t oled_write_commands(const uint8_t *commands, size_t count)
{
    return oled_write(0x00u, commands, count);
}

/*******************************************************************************
* Function Name: oled_clear_buffer
********************************************************************************
* Summary:
*  Clears the local display framebuffer.
*******************************************************************************/
static void oled_clear_buffer(void)
{
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

/*******************************************************************************
* Function Name: oled_upper_copy
********************************************************************************
* Summary:
*  Copies a string into a destination buffer while converting it to upper-case.
*******************************************************************************/
static void oled_upper_copy(char *dst, size_t dst_size, const char *src)
{
    size_t out = 0u;

    if ((dst_size == 0u) || (src == NULL))
    {
        return;
    }

    while ((src[out] != '\0') && (out < (dst_size - 1u)))
    {
        char ch = src[out];
        if ((ch >= 'a') && (ch <= 'z'))
        {
            ch = (char)(ch - ('a' - 'A'));
        }
        dst[out] = ch;
        out++;
    }

    dst[out] = '\0';
}

/*******************************************************************************
* Function Name: oled_glyph
********************************************************************************
* Summary:
*  Produces a 5-column bitmap for a supported ASCII glyph.
*******************************************************************************/
static void oled_glyph(char c, uint8_t glyph[5])
{
    memset(glyph, 0, 5u);

    if ((c >= 'a') && (c <= 'z'))
    {
        c = (char)(c - ('a' - 'A'));
    }

    switch (c)
    {
        case 'A': glyph[0]=0x7E; glyph[1]=0x11; glyph[2]=0x11; glyph[3]=0x11; glyph[4]=0x7E; break;
        case 'B': glyph[0]=0x7F; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x36; break;
        case 'C': glyph[0]=0x3E; glyph[1]=0x41; glyph[2]=0x41; glyph[3]=0x41; glyph[4]=0x22; break;
        case 'D': glyph[0]=0x7F; glyph[1]=0x41; glyph[2]=0x41; glyph[3]=0x22; glyph[4]=0x1C; break;
        case 'E': glyph[0]=0x7F; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x41; break;
        case 'F': glyph[0]=0x7F; glyph[1]=0x09; glyph[2]=0x09; glyph[3]=0x09; glyph[4]=0x01; break;
        case 'G': glyph[0]=0x3E; glyph[1]=0x41; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x7A; break;
        case 'H': glyph[0]=0x7F; glyph[1]=0x08; glyph[2]=0x08; glyph[3]=0x08; glyph[4]=0x7F; break;
        case 'I': glyph[0]=0x00; glyph[1]=0x41; glyph[2]=0x7F; glyph[3]=0x41; glyph[4]=0x00; break;
        case 'J': glyph[0]=0x20; glyph[1]=0x40; glyph[2]=0x41; glyph[3]=0x3F; glyph[4]=0x01; break;
        case 'K': glyph[0]=0x7F; glyph[1]=0x08; glyph[2]=0x14; glyph[3]=0x22; glyph[4]=0x41; break;
        case 'L': glyph[0]=0x7F; glyph[1]=0x40; glyph[2]=0x40; glyph[3]=0x40; glyph[4]=0x40; break;
        case 'M': glyph[0]=0x7F; glyph[1]=0x02; glyph[2]=0x0C; glyph[3]=0x02; glyph[4]=0x7F; break;
        case 'N': glyph[0]=0x7F; glyph[1]=0x04; glyph[2]=0x08; glyph[3]=0x10; glyph[4]=0x7F; break;
        case 'O': glyph[0]=0x3E; glyph[1]=0x41; glyph[2]=0x41; glyph[3]=0x41; glyph[4]=0x3E; break;
        case 'P': glyph[0]=0x7F; glyph[1]=0x09; glyph[2]=0x09; glyph[3]=0x09; glyph[4]=0x06; break;
        case 'Q': glyph[0]=0x3E; glyph[1]=0x41; glyph[2]=0x51; glyph[3]=0x21; glyph[4]=0x5E; break;
        case 'R': glyph[0]=0x7F; glyph[1]=0x09; glyph[2]=0x19; glyph[3]=0x29; glyph[4]=0x46; break;
        case 'S': glyph[0]=0x46; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x31; break;
        case 'T': glyph[0]=0x01; glyph[1]=0x01; glyph[2]=0x7F; glyph[3]=0x01; glyph[4]=0x01; break;
        case 'U': glyph[0]=0x3F; glyph[1]=0x40; glyph[2]=0x40; glyph[3]=0x40; glyph[4]=0x3F; break;
        case 'V': glyph[0]=0x1F; glyph[1]=0x20; glyph[2]=0x40; glyph[3]=0x20; glyph[4]=0x1F; break;
        case 'W': glyph[0]=0x7F; glyph[1]=0x20; glyph[2]=0x18; glyph[3]=0x20; glyph[4]=0x7F; break;
        case 'X': glyph[0]=0x63; glyph[1]=0x14; glyph[2]=0x08; glyph[3]=0x14; glyph[4]=0x63; break;
        case 'Y': glyph[0]=0x03; glyph[1]=0x04; glyph[2]=0x78; glyph[3]=0x04; glyph[4]=0x03; break;
        case 'Z': glyph[0]=0x61; glyph[1]=0x51; glyph[2]=0x49; glyph[3]=0x45; glyph[4]=0x43; break;
        case '0': glyph[0]=0x3E; glyph[1]=0x45; glyph[2]=0x49; glyph[3]=0x51; glyph[4]=0x3E; break;
        case '1': glyph[0]=0x00; glyph[1]=0x21; glyph[2]=0x7F; glyph[3]=0x01; glyph[4]=0x00; break;
        case '2': glyph[0]=0x21; glyph[1]=0x43; glyph[2]=0x45; glyph[3]=0x49; glyph[4]=0x31; break;
        case '3': glyph[0]=0x42; glyph[1]=0x41; glyph[2]=0x51; glyph[3]=0x69; glyph[4]=0x46; break;
        case '4': glyph[0]=0x0C; glyph[1]=0x14; glyph[2]=0x24; glyph[3]=0x7F; glyph[4]=0x04; break;
        case '5': glyph[0]=0x72; glyph[1]=0x51; glyph[2]=0x51; glyph[3]=0x51; glyph[4]=0x4E; break;
        case '6': glyph[0]=0x1E; glyph[1]=0x29; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x06; break;
        case '7': glyph[0]=0x40; glyph[1]=0x47; glyph[2]=0x48; glyph[3]=0x50; glyph[4]=0x60; break;
        case '8': glyph[0]=0x36; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x36; break;
        case '9': glyph[0]=0x30; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x4A; glyph[4]=0x3C; break;
        case '!': glyph[0]=0x00; glyph[1]=0x00; glyph[2]=0x5F; glyph[3]=0x00; glyph[4]=0x00; break;
        case '?': glyph[0]=0x02; glyph[1]=0x01; glyph[2]=0x51; glyph[3]=0x09; glyph[4]=0x06; break;
        case ',': glyph[0]=0x00; glyph[1]=0x40; glyph[2]=0x20; glyph[3]=0x00; glyph[4]=0x00; break;
        case ';': glyph[0]=0x00; glyph[1]=0x56; glyph[2]=0x36; glyph[3]=0x00; glyph[4]=0x00; break;
        case '-': glyph[0]=0x08; glyph[1]=0x08; glyph[2]=0x08; glyph[3]=0x08; glyph[4]=0x08; break;
        case '+': glyph[0]=0x08; glyph[1]=0x08; glyph[2]=0x3E; glyph[3]=0x08; glyph[4]=0x08; break;
        case '=': glyph[0]=0x14; glyph[1]=0x14; glyph[2]=0x14; glyph[3]=0x14; glyph[4]=0x14; break;
        case '.': glyph[0]=0x00; glyph[1]=0x60; glyph[2]=0x60; glyph[3]=0x00; glyph[4]=0x00; break;
        case ':': glyph[0]=0x00; glyph[1]=0x36; glyph[2]=0x36; glyph[3]=0x00; glyph[4]=0x00; break;
        case '/': glyph[0]=0x03; glyph[1]=0x04; glyph[2]=0x08; glyph[3]=0x10; glyph[4]=0x60; break;
        case '%': glyph[0]=0x63; glyph[1]=0x13; glyph[2]=0x08; glyph[3]=0x64; glyph[4]=0x63; break;
        case '&': glyph[0]=0x36; glyph[1]=0x49; glyph[2]=0x55; glyph[3]=0x22; glyph[4]=0x50; break;
        case '#': glyph[0]=0x14; glyph[1]=0x3E; glyph[2]=0x14; glyph[3]=0x3E; glyph[4]=0x14; break;
        case '_': glyph[0]=0x40; glyph[1]=0x40; glyph[2]=0x40; glyph[3]=0x40; glyph[4]=0x40; break;
        case '(': glyph[0]=0x00; glyph[1]=0x1C; glyph[2]=0x22; glyph[3]=0x41; glyph[4]=0x00; break;
        case ')': glyph[0]=0x00; glyph[1]=0x41; glyph[2]=0x22; glyph[3]=0x1C; glyph[4]=0x00; break;
        case '\'': glyph[0]=0x00; glyph[1]=0x03; glyph[2]=0x07; glyph[3]=0x00; glyph[4]=0x00; break;
        case '"': glyph[0]=0x00; glyph[1]=0x07; glyph[2]=0x00; glyph[3]=0x07; glyph[4]=0x00; break;
        case ' ': break;
        default:  glyph[0]=0x02; glyph[1]=0x01; glyph[2]=0x59; glyph[3]=0x09; glyph[4]=0x06; break;
    }

    if ((c >= '0') && (c <= '9'))
    {
        for (uint32_t i = 0; i < 5u; ++i)
        {
            glyph[i] = oled_reverse_bits(glyph[i]);
        }
    }
}

/*******************************************************************************
* Function Name: oled_draw_char
********************************************************************************
* Summary:
*  Draws a single glyph into the local framebuffer.
*******************************************************************************/
static void oled_draw_char(uint8_t row, uint8_t col, char c)
{
    uint8_t glyph[5];
    uint16_t x = (uint16_t)col * OLED_CHAR_WIDTH;
    uint16_t index = ((uint16_t)row * OLED_WIDTH) + x;

    if ((row >= OLED_TEXT_ROWS) || (col >= OLED_TEXT_COLS))
    {
        return;
    }

    oled_glyph(c, glyph);
    for (uint32_t i = 0; i < 5u; ++i)
    {
        oled_buffer[index + i] = glyph[i];
    }
    oled_buffer[index + 5u] = 0x00u;
}

/*******************************************************************************
* Function Name: oled_draw_text
********************************************************************************
* Summary:
*  Draws a single line of text into the local framebuffer.
*******************************************************************************/
static void oled_draw_text(uint8_t row, const char *text)
{
    char normalized[OLED_TEXT_COLS + 1u];
    size_t len = 0u;

    if ((row >= OLED_TEXT_ROWS) || (text == NULL))
    {
        return;
    }

    oled_upper_copy(normalized, sizeof(normalized), text);
    len = strlen(normalized);
    if (len > OLED_TEXT_COLS)
    {
        len = OLED_TEXT_COLS;
        normalized[len] = '\0';
    }

    for (size_t col = 0; col < len; ++col)
    {
        oled_draw_char(row, (uint8_t)col, normalized[col]);
    }
}

/*******************************************************************************
* Function Name: oled_draw_wrapped
********************************************************************************
* Summary:
*  Draws wrapped text, moving whole words to the next row when required.
*
* Return:
*  uint8_t: The next free text row after the rendered text.
*******************************************************************************/
static uint8_t oled_draw_wrapped(uint8_t start_row, const char *text)
{
    char normalized[96];
    size_t index = 0u;
    uint8_t row = start_row;

    if (text == NULL)
    {
        return row;
    }

    oled_upper_copy(normalized, sizeof(normalized), text);

    while ((normalized[index] != '\0') && (row < OLED_TEXT_ROWS))
    {
        char line[OLED_TEXT_COLS + 1u];
        size_t line_len = 0u;
        size_t last_space = 0u;
        size_t word_start = index;

        while ((normalized[index] == ' ') || (normalized[index] == '\n'))
        {
            if (normalized[index] == '\n')
            {
                index++;
                break;
            }
            index++;
        }

        word_start = index;
        while ((normalized[index] != '\0') &&
               (normalized[index] != '\n') &&
               (line_len < OLED_TEXT_COLS))
        {
            line[line_len++] = normalized[index++];
            if (line[line_len - 1u] == ' ')
            {
                last_space = line_len;
            }
        }

        if ((normalized[index] != '\0') &&
            (normalized[index] != '\n') &&
            (line_len == OLED_TEXT_COLS) &&
            (last_space != 0u))
        {
            index = word_start + last_space;
            line_len = last_space;
        }

        while ((line_len > 0u) && (line[line_len - 1u] == ' '))
        {
            line_len--;
        }
        line[line_len] = '\0';
        oled_draw_text(row, line);
        row++;

        if (normalized[index] == '\n')
        {
            index++;
        }
    }

    return row;
}

/*******************************************************************************
* Function Name: oled_show_screen
********************************************************************************
* Summary:
*  Renders a compact multi-line screen layout and flushes it to the OLED.
*******************************************************************************/
static void oled_show_screen(const char *title,
                             const char *line1,
                             const char *line2,
                             const char *line3,
                             const char *line4)
{
    uint8_t next_row = 0u;

    if (!oled_ready)
    {
        return;
    }

    oled_clear_buffer();
    oled_draw_text(next_row++, title);
    next_row++;
    next_row = oled_draw_wrapped(next_row, line1);
    next_row = oled_draw_wrapped(next_row, line2);
    next_row = oled_draw_wrapped(next_row, line3);
    (void)oled_draw_wrapped(next_row, line4);
    (void)oled_flush();
}

/*******************************************************************************
* Function Name: oled_flush
********************************************************************************
* Summary:
*  Sends the full local framebuffer to the OLED.
*******************************************************************************/
static cy_rslt_t oled_flush(void)
{
    static const uint8_t address_window[] = { 0x21u, 0x00u, 0x7Fu, 0x22u, 0x00u, 0x07u };
    cy_rslt_t result = oled_write_commands(address_window, sizeof(address_window));
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }

    return oled_write(0x40u, oled_buffer, sizeof(oled_buffer));
}

/*******************************************************************************
* Function Name: display_status_init
********************************************************************************
* Summary:
*  Initializes the I2C-connected OLED controller and clears the display.
*******************************************************************************/
void display_status_init(void)
{
    cy_rslt_t result;
    cyhal_i2c_cfg_t cfg =
    {
        .is_slave = false,
        .address = 0u,
        .frequencyhal_hz = OLED_I2C_FREQUENCY_HZ
    };

    static const uint8_t init_sequence[] =
    {
        0xAEu, 0xD5u, 0x80u, 0xA8u, 0x3Fu, 0xD3u, 0x00u, 0x40u,
        0x8Du, 0x14u, 0x20u, 0x00u, 0xA1u, 0xC8u, 0xDAu, 0x12u,
        0x81u, 0x7Fu, 0xD9u, 0xF1u, 0xDBu, 0x40u, 0xA4u, 0xA6u, 0xAFu
    };

    result = cyhal_i2c_init(&oled_i2c, CYBSP_I2C_SDA, CYBSP_I2C_SCL, NULL);
    if (result != CY_RSLT_SUCCESS)
    {
        return;
    }

    result = cyhal_i2c_configure(&oled_i2c, &cfg);
    if (result != CY_RSLT_SUCCESS)
    {
        cyhal_i2c_free(&oled_i2c);
        return;
    }

    result = oled_write_commands(init_sequence, sizeof(init_sequence));
    if (result != CY_RSLT_SUCCESS)
    {
        cyhal_i2c_free(&oled_i2c);
        return;
    }

    oled_ready = true;
    oled_clear_buffer();
    (void)oled_flush();
}

/*******************************************************************************
* Function Name: display_status_show_boot
********************************************************************************
* Summary:
*  Displays a boot/status splash screen.
*******************************************************************************/
void display_status_show_boot(const char *line1, const char *line2)
{
    oled_show_screen("WI-FI SERVER", line1, line2, NULL, NULL);
}

/*******************************************************************************
* Function Name: display_status_show_known_network_attempt
********************************************************************************
* Summary:
*  Displays the currently attempted stored Wi-Fi network during auto-connect.
*******************************************************************************/
void display_status_show_known_network_attempt(const char *ssid, uint32_t index, uint32_t total)
{
    char attempt_line[24];
    char ssid_line[24];

    snprintf(attempt_line, sizeof(attempt_line), "KNOWN %lu/%lu",
             (unsigned long)index, (unsigned long)total);
    snprintf(ssid_line, sizeof(ssid_line), "SSID: %s", ssid);

    oled_show_screen("AUTO CONNECT", ssid_line, attempt_line, NULL, NULL);
}

/*******************************************************************************
* Function Name: display_status_show_scanning
********************************************************************************
* Summary:
*  Displays Wi-Fi scan progress.
*******************************************************************************/
void display_status_show_scanning(void)
{
    oled_show_screen("PROVISIONING", "SCANNING WIFI", "PLEASE WAIT", NULL, NULL);
}

/*******************************************************************************
* Function Name: display_status_show_connecting
********************************************************************************
* Summary:
*  Displays an in-progress Wi-Fi connection attempt.
*******************************************************************************/
void display_status_show_connecting(const char *ssid, uint32_t attempt, uint32_t total)
{
    char attempt_line[24];
    char ssid_line[24];

    snprintf(attempt_line, sizeof(attempt_line), "TRY %lu/%lu",
             (unsigned long)attempt, (unsigned long)total);
    snprintf(ssid_line, sizeof(ssid_line), "SSID %s", ssid);

    oled_show_screen("CONNECTING", ssid_line, attempt_line, NULL, NULL);
}

/*******************************************************************************
* Function Name: display_status_show_connect_success
********************************************************************************
* Summary:
*  Displays the successful Wi-Fi connection state and access details.
*******************************************************************************/
void display_status_show_connect_success(const char *ssid, const char *ip, const char *hostname)
{
    char ssid_line[24];
    char ip_line[24];
    char host_line[24];

    snprintf(ssid_line, sizeof(ssid_line), "SSID %s", ssid);
    snprintf(ip_line, sizeof(ip_line), "IP %s", ip);
    snprintf(host_line, sizeof(host_line), "%s", hostname);

    oled_show_screen("SUCCESS", ssid_line, ip_line, host_line, NULL);
}

/*******************************************************************************
* Function Name: display_status_show_connect_failure
********************************************************************************
* Summary:
*  Displays a failed Wi-Fi connection attempt.
*******************************************************************************/
void display_status_show_connect_failure(const char *ssid, const char *reason)
{
    char ssid_line[24];
    char reason_line[24];

    snprintf(ssid_line, sizeof(ssid_line), "SSID %s", ssid);
    snprintf(reason_line, sizeof(reason_line), "%s", reason);

    oled_show_screen("CONNECT FAIL", ssid_line, reason_line, NULL, NULL);
}

/*******************************************************************************
* Function Name: display_status_show_provisioning
********************************************************************************
* Summary:
*  Displays SoftAP provisioning instructions and credentials.
*******************************************************************************/
void display_status_show_provisioning(const char *softap_ssid, const char *softap_password,
                                      const char *ap_ip, const char *hostname)
{
    char ssid_line[24];
    char pwd_line[24];
    char ip_line[24];

    snprintf(ssid_line, sizeof(ssid_line), "AP %s", softap_ssid);
    snprintf(pwd_line, sizeof(pwd_line), "PWD %s", softap_password);
    snprintf(ip_line, sizeof(ip_line), "IP %s", ap_ip);
    (void)hostname;

    oled_show_screen("PROVISIONING", ssid_line, pwd_line, ip_line, NULL);
}

/*******************************************************************************
* Function Name: display_status_show_sta_ready
********************************************************************************
* Summary:
*  Displays STA-mode access information after the device is ready.
*******************************************************************************/
void display_status_show_sta_ready(const char *sta_ssid, const char *sta_ip, const char *hostname)
{
    char ip_line[24];
    char host_line[24];

    (void)sta_ssid;
    snprintf(ip_line, sizeof(ip_line), "IP %s", sta_ip);
    snprintf(host_line, sizeof(host_line), "%s", hostname);

    oled_show_screen("DEVICE READY", ip_line, host_line, NULL, NULL);
}

/*******************************************************************************
* Function Name: display_status_show_sta_status
********************************************************************************
* Summary:
*  Displays the live LIN actuator runtime status text.
*******************************************************************************/
void display_status_show_sta_status(const char *status_text)
{
    char plain_status[256];

    if (!oled_ready)
    {
        return;
    }

    for (uint8_t row = OLED_STATUS_START_ROW; row < OLED_TEXT_ROWS; ++row)
    {
        memset(&oled_buffer[row * OLED_WIDTH], 0, OLED_WIDTH);
    }

    oled_prepare_status_text(plain_status, sizeof(plain_status), status_text);
    (void)oled_draw_wrapped(OLED_STATUS_START_ROW, plain_status);
    (void)oled_flush();
}

#else

void display_status_init(void) {}
void display_status_show_boot(const char *line1, const char *line2) { (void)line1; (void)line2; }
void display_status_show_known_network_attempt(const char *ssid, uint32_t index, uint32_t total)
{
    (void)ssid;
    (void)index;
    (void)total;
}
void display_status_show_scanning(void) {}
void display_status_show_connecting(const char *ssid, uint32_t attempt, uint32_t total)
{
    (void)ssid;
    (void)attempt;
    (void)total;
}
void display_status_show_connect_success(const char *ssid, const char *ip, const char *hostname)
{
    (void)ssid;
    (void)ip;
    (void)hostname;
}
void display_status_show_connect_failure(const char *ssid, const char *reason)
{
    (void)ssid;
    (void)reason;
}
void display_status_show_provisioning(const char *softap_ssid, const char *softap_password,
                                      const char *ap_ip, const char *hostname)
{
    (void)softap_ssid;
    (void)softap_password;
    (void)ap_ip;
    (void)hostname;
}
void display_status_show_sta_ready(const char *sta_ssid, const char *sta_ip, const char *hostname)
{
    (void)sta_ssid;
    (void)sta_ip;
    (void)hostname;
}
void display_status_show_sta_status(const char *status_text) { (void)status_text; }

#endif /* ENABLE_OLED */
