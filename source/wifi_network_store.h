#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cy_result.h"

#define WIFI_SSID_MAX_LEN  (32)
#define WIFI_PWD_MAX_LEN   (64)

/* Tune this: how many networks you want to remember */
#define WIFI_STORE_MAX_NETWORKS (8u)

typedef struct
{
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char pwd[WIFI_PWD_MAX_LEN + 1];
} wifi_network_t;

cy_rslt_t wifi_store_init(void);

/* Get networks ordered by most-recent-first */
cy_rslt_t wifi_store_get_known_networks(wifi_network_t* out_list,
                                        uint32_t max_list,
                                        uint32_t* out_count);

/* Save/update a network; evict LRU if full */
cy_rslt_t wifi_store_save_network(const char* ssid, const char* pwd);

/* Mark a network as recently used (optional; call on successful connect) */
cy_rslt_t wifi_store_mark_used(const char* ssid);