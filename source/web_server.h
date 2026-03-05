/******************************************************************************
* File Name: web_server.h
*
* Description: This file contains configuration parameters for configuring the
*              device in SoftAP mode and start HTTP server.
*
********************************************************************************
 * (c) 2021-2025, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 * This software, associated documentation and materials ("Software") is
 * owned by Infineon Technologies AG or one of its affiliates ("Infineon")
 * and is protected by and subject to worldwide patent protection, worldwide
 * copyright laws, and international treaty provisions. Therefore, you may use
 * this Software only as provided in the license agreement accompanying the
 * software package from which you obtained this Software. If no license
 * agreement applies, then any use, reproduction, modification, translation, or
 * compilation of this Software is prohibited without the express written
 * permission of Infineon.
 *
 * Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
 * IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
 * THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
 * SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
 * Infineon reserves the right to make changes to the Software without notice.
 * You are responsible for properly designing, programming, and testing the
 * functionality and safety of your intended application of the Software, as
 * well as complying with any legal requirements related to its use. Infineon
 * does not guarantee that the Software will be free from intrusion, data theft
 * or loss, or other breaches ("Security Breaches"), and Infineon shall have
 * no liability arising out of any Security Breaches. Unless otherwise
 * explicitly approved by Infineon, the Software may not be used in any
 * application where a failure of the Product or any consequences of the use
 * thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

#ifndef WEB_SERVER_DEMO_H_
#define WEB_SERVER_DEMO_H_

#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* Wi-Fi connection manager header files */
#include "cy_wcm.h"
#include "cybsp.h"
//#include "cy_lwip.h"
#include "cyhal_gpio.h"

/* CAPSENSE header file */
#include "cycfg_capsense.h"

#include "cyabs_rtos.h"
#include "cy_http_server.h"
#include "html_web_page.h"
#include "sensors.h"

#ifdef ENABLE_TFT
/* CY8CKIT-028-TFT shield and LCD library */
#include "GUI.h"
#include "mtb_st7789v.h"
#include "cy8ckit_028_tft_pins.h"
#include "mtb_light_sensor.h"
#endif /* #ifdef ENABLE_TFT */


/* Button used to force provisioning mode. Most BSPs define CYBSP_USER_BTN. */
#ifndef PROVISION_FORCE_BUTTON
#define PROVISION_FORCE_BUTTON   (CYBSP_USER_BTN)
#endif

/* Many PSoC kits use active-low user buttons (pressed = 0). */
#ifndef PROVISION_BUTTON_ACTIVE_STATE
#define PROVISION_BUTTON_ACTIVE_STATE (0u)
#endif

/* Long-press duration (milliseconds) to trigger reprovision at runtime. */
#define PROVISION_LONG_PRESS_MS (2000u)
#define PROVISION_BUTTON_DEBOUNCE_MS (50u)
#define PROVISION_BUTTON_INTR_PRIORITY (7u)
#define SERVER_LOOP_PERIOD_MS     (50u)


#define INITIALISER_IPV4_ADDRESS(addr_var, addr_val)  addr_var = { CY_WCM_IP_VER_V4, { .v4 = (uint32_t)(addr_val) } }
#define MAKE_IPV4_ADDRESS(a, b, c, d)                 ((((uint32_t) d) << 24) | (((uint32_t) c) << 16) | \
                                                       (((uint32_t) b) << 8) |((uint32_t) a))

#define CHECK_RESULT(x)                                do { if (CY_RSLT_SUCCESS != x) { CY_ASSERT(0); } } while(0);
#define APP_INFO(x)                                    do { printf("Info: "); printf x; } while(0);
#define ERR_INFO(x)                                    do { printf("Error: "); printf x; } while(0);
#define PRINT_AND_ASSERT(result, msg, args...)         do                                 \
                                                      {                                   \
                                                           if (CY_RSLT_SUCCESS != result) \
                                                           {                              \
                                                               ERR_INFO((msg, ## args));  \
                                                               CY_ASSERT(0);              \
                                                           }                              \
                                                      } while(0);

#define HTTP_PORT                                    (80u)
#define URL_LENGTH                                   (128)
#define MAX_SOCKETS                                  (4)
#define MAX_HTTP_RESPONSE_LENGTH                     (sizeof(HTTP_SOFTAP_STARTUP_WEBPAGE) + 64)
#define HTTP_REQUEST_HANDLE_SUCCESS                  (0)
#define HTTP_REQUEST_HANDLE_ERROR                    (-1)
#define DEVICE_DATA_RESPONSE_LENGTH                  (sizeof(SOFTAP_DEVICE_DATA) + 64)
#define WIFI_CONNECT_RESPONSE_LENGTH                 (sizeof(WIFI_CONNECT_RESPONSE_START) + sizeof(WIFI_CONNECT_SUCCESS_RESPONSE_END) + sizeof(WIFI_CONNECT_IN_PROGRESS) + 140)

#define BUFFER_LENGTH                                (2048)
#define WIFI_SSID_LEN                                (32u)
#define WIFI_PWD_LEN                                 (64u)
#define MAX_WIFI_SCAN_HTTP_RESPONSE_LENGTH           (2048)

#define SENSOR_BUFFER_LENGTH                         (128)
#define DISPLAY_BUFFER_LENGTH                        (64)

/* SoftAP Credentials */
#define SOFTAP_SSID                                  "SOFTAP_SSID"

/* The password length should meet the requirement of the configured security
 * type. e.g. Passworld length should be between 8-63 characters for
 * CY_WCM_SECURITY_WPA2_AES_PSK.
 */
#define SOFTAP_PASSWORD                              "SOFTAP_PWD"
#define SOFTAP_SECURITY_TYPE                         CY_WCM_SECURITY_WPA2_AES_PSK
#define SOFTAP_IP_ADDRESS                            MAKE_IPV4_ADDRESS(192, 168, 0,  2)
#define SOFTAP_NETMASK                               MAKE_IPV4_ADDRESS(255, 255, 255, 0)
#define SOFTAP_GATEWAY                               MAKE_IPV4_ADDRESS(192, 168, 0,  2)

#define MAX_WIFI_RETRY_COUNT                         (6u)
#define WIFI_CONN_RETRY_INTERVAL_MSEC                (250u)

/* HTTP headers used in response to client */
#define HTTP_HEADER_204                              "HTTP/1.1 204 No Content"

/* The delay in milliseconds between successive scans.*/
#define SCAN_DELAY_MS                                (5000u)

/* The delay in milliseconds between successive data upload.*/
#define WIFI_DATA_UPLOAD_INTERVAL_MSEC               (50u)

/* Initial row position on TFT display */
#define TOP_DISPLAY                                  (0u)

/* Offset row position on TFT display */
#define ROW_OFFSET                                   (20)
/* Offset row position on TFT display for displaying sensor value */
#define SENSOR_DISPLAY_OFFSET                        (200)

/* Macros used to format HTTP event stream sent from server to client */
#define EVENT_STREAM_DATA                            "data: "
#define LFLF                                         "\n\n"
#define CHUNKED_CONTENT_LENGTH                       (0u)

#define INCREASE                                     ("Increase")
#define DECREASE                                     ("Decrease")

#define MAKE_IP_PARAMETERS(a, b, c, d)               ((((uint32_t) d) << 24) | \
                                                     (((uint32_t) c) << 16) | \
                                                     (((uint32_t) b) << 8) | \
                                                     ((uint32_t) a))

#define SERVER_RECONFIGURE_REQUESTED                 (1u)
#define SERVER_RECONFIGURED                          (2u)

/* The size of the cy_wcm_ip_address_t array that is passed to
 * cy_wcm_get_ip_addr API. In the case of stand-alone AP or STA mode, the size of
 * the array is 1. In concurrent AP/STA mode, the size of the array is 2 where
 * the first index stores the IP address of the STA and the second index
 * stores the IP address of the AP.
 */
#define SIZE_OF_IP_ARRAY_STA                        (1u)


/* Macros that defines ASCII characters used in the code example */
#define CAPITAL_LETTER_A_ASCII_VALUE                (65u)
#define SMALL_LETTER_A_ASCII_VALUE                  (97u)
#define NUMBER_ZERO_ASCII_VALUE                     (48u)
#define LF_OPERATOR_ASCII_VALUE                     (10u)
#define DLE_OPERATOR_ASCII_VALUE                    (16u)
#define MODUS_OPERATOR_ASCII_VALUE                  (37u)
#define PLUS_OPERATOR_ASCII_VALUE                   (43u)
#define EQUALS_OPERATOR_ASCII_VALUE                 (61u)
#define AMPERSAND_OPERATOR_ASCII_VALUE              (38u)
#define SPACE_CHARACTER_ASCII_VALUE                 (32u)
#define NULL_CHARACTER_ASCII_VALUE                  (0u)
#define VALID_CHARACTER_ASCII_VALUE                 (128u)
#define URL_DECODE_ASCII_OFFSET_VALUE               (3u)



void server_task(void *arg);
cy_rslt_t wifi_extract_credentials(const uint8_t *data, uint32_t data_len, cy_http_response_stream_t *stream);
cy_rslt_t start_sta_mode(void);
cy_rslt_t start_ap_mode(void);
void scan_for_available_aps(cy_http_response_stream_t *url_stream);
void url_decode(char *dst, const uint8_t *src);
void initialize_display(void);
void display_configuration(void);
cy_rslt_t configure_http_server(void);
cy_rslt_t reconfigure_http_server(void);

#endif /* WEB_SERVER_DEMO_H_ */

/* [] END OF FILE */
