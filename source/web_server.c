/*******************************************************************************
* File Name: web_server.c
*
* Description: This file contains the necessary functions to configure the device 
*              in SoftAP mode and starts an HTTP server. The device can be 
*              provisioned to connect to an AP, after performing a scan for 
*              available APs, by using the credentials entered via HTTP client. 
*              Once the device is connected to AP, starts an HTTP server which 
*              processes GET and POST request from the HTTP client. 
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

/* Header file includes */
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* FreeRTOS header file */
#include <FreeRTOS.h>
#include <task.h>

/* Secure Sockets header file */
#include "cy_secure_sockets.h"
#include "cy_tls.h"

/* Wi-Fi connection manager header files */
#include "cy_wcm.h"
#include "cy_wcm_error.h"

/* HTTP server task header file. */
#include "web_server.h"
#include "cy_http_server.h"
#include "cy_log.h"

/* Standard C header file */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "wifi_network_store.h"
#include "display_status.h"

#include "cy_network_mw_core.h"

#include "LIN_actuator.h"

#if LWIP_MDNS_RESPONDER
#include "lwip/apps/mdns.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#endif

/*******************************************************************************
* Global Variables
********************************************************************************/
static const cy_wcm_ip_setting_t ap_sta_mode_ip_settings =
{
    INITIALISER_IPV4_ADDRESS( .ip_address, SOFTAP_IP_ADDRESS),
    INITIALISER_IPV4_ADDRESS( .netmask,    SOFTAP_NETMASK),
    INITIALISER_IPV4_ADDRESS( .gateway,    SOFTAP_GATEWAY),
};

/* Holds the IP address and port number details of the socket for the HTTP server. */
cy_socket_sockaddr_t http_server_ip_address;

/* Pointer to HTTP event stream used to send device data to client. */
cy_http_response_stream_t* http_event_stream;

/* Wi-Fi network interface. */
cy_network_interface_t nw_interface;

/* HTTP server instance. */
cy_http_server_t http_ap_server;

/* HTTP server instance. */
cy_http_server_t http_sta_server;

/*Buffer to store SSID*/
uint8_t wifi_ssid[WIFI_SSID_LEN] = {0};

/*Buffer to store Password*/
uint8_t wifi_pwd[WIFI_PWD_LEN] = {0}; 

/*Buffer to store HTTP data*/
char buffer[BUFFER_LENGTH] = {0};

/* Holds the response handler for HTTP GET and POST request from the client 
* to implement Wi-Fi scan and Wi-Fi connect funtionality. 
*/
cy_resource_dynamic_data_t http_wifi_resource;

/* Flag to indicate if scan has completed.*/
volatile bool scan_complete_flag = false;

/* Flag to indicate if device has been configured. */
volatile bool device_configured = false;

/* Buffer to store ssid  */
static char ssid_buff[BUFFER_LENGTH];

/*Variable to indicate re-configuration request*/
volatile int8_t reconfiguration_request = 0;

/* Array to store Wi-Fi connect response. */
static char http_wifi_connect_response[WIFI_CONNECT_RESPONSE_LENGTH] = {0};

/* Array to store Wi-Fi scan response. */
static char http_scan_response[MAX_WIFI_SCAN_HTTP_RESPONSE_LENGTH] = {0};

/* True when provisioning button GPIO is initialized and safe to read. */
static bool provisioning_button_enabled = false;
/* True when GPIO interrupt for provisioning button is enabled. */
static bool provisioning_button_irq_enabled = false;

/* ISR-to-task signaling flags for provisioning button events. */
static volatile bool provisioning_button_irq_pressed = false;
static volatile bool provisioning_button_irq_released = false;
static volatile bool provisioning_button_irq_is_pressed = false;
static volatile bool provisioning_button_force_mode_requested = false;
static cyhal_gpio_event_t provisioning_button_press_event = CYHAL_GPIO_IRQ_FALL;
static cyhal_gpio_event_t provisioning_button_release_event = CYHAL_GPIO_IRQ_RISE;

/* Known join failure observed from WHD when authentication/credentials are invalid. */
#define WIFI_JOIN_AUTH_FAILURE_RSLT                  (33555456u)
#define WIFI_CONN_BACKOFF_MAX_MSEC                   (5000u)

typedef enum
{
    WIFI_FAIL_REASON_NONE = 0,
    WIFI_FAIL_REASON_BUTTON_ABORT,
    WIFI_FAIL_REASON_AUTH_CREDENTIALS,
    WIFI_FAIL_REASON_CONNECT_TIMEOUT,
    WIFI_FAIL_REASON_AP_UNREACHABLE,
    WIFI_FAIL_REASON_UNKNOWN
} wifi_fail_reason_t;

static volatile wifi_fail_reason_t last_wifi_fail_reason = WIFI_FAIL_REASON_NONE;

#if LWIP_MDNS_RESPONDER
/* mDNS state for STA mode hostname advertisement. */
static bool sta_mdns_initialized = false;
static bool sta_mdns_active = false;
static s8_t sta_mdns_http_service_slot = -1;
static uint32_t sta_mdns_retry_elapsed_ms = 0u;
#endif

/* Forward declarations for runtime button/provisioning control. */
static void handle_runtime_force_provisioning(uint32_t loop_period_ms);
static bool is_provisioning_ap_active(void);
static void provisioning_button_isr_callback(void *callback_arg, cyhal_gpio_event_t event);
static wifi_fail_reason_t classify_wifi_connect_failure(cy_rslt_t result_code);
static const char* wifi_fail_reason_to_text(wifi_fail_reason_t reason);
static cy_rslt_t start_sta_mdns_service(void);
static void stop_sta_mdns_service(void);
static void format_ipv4_address(uint32_t ip_addr, char *buffer, size_t buffer_len);
static size_t append_text(char *dst, size_t dst_size, size_t offset, const char *src);
static void service_sta_mdns_retry(uint32_t elapsed_ms);

static cyhal_gpio_callback_data_t provisioning_button_cb_data =
{
    .callback = provisioning_button_isr_callback,
    .callback_arg = NULL
};

/*******************************************************************************
* Function Name: append_text
********************************************************************************
* Summary:
*  Appends a NUL-terminated string into a bounded destination buffer.
*******************************************************************************/
static size_t append_text(char *dst, size_t dst_size, size_t offset, const char *src)
{
    int written;

    if ((dst == NULL) || (src == NULL) || (offset >= dst_size))
    {
        return offset;
    }

    written = snprintf(dst + offset, dst_size - offset, "%s", src);
    if (written < 0)
    {
        return offset;
    }

    if ((size_t)written >= (dst_size - offset))
    {
        return dst_size - 1u;
    }

    return offset + (size_t)written;
}

/*******************************************************************************
* Function Name: is_provisioning_ap_active
********************************************************************************
* Summary:
*  Returns true if SoftAP/provisioning interface is already active.
*******************************************************************************/
static bool is_provisioning_ap_active(void)
{
    cy_wcm_ip_address_t ap_ip;
    return (cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_AP, &ap_ip) == CY_RSLT_SUCCESS);
}

/*******************************************************************************
* Function Name: provisioning_button_isr_callback
********************************************************************************
* Summary:
*  GPIO interrupt callback for provisioning button.
*  Signals main context about button press/release state.
*******************************************************************************/
static void provisioning_button_isr_callback(void *callback_arg, cyhal_gpio_event_t event)
{
    (void)callback_arg;

    if ((event & provisioning_button_press_event) != 0u)
    {
        provisioning_button_irq_is_pressed = true;
        provisioning_button_irq_pressed = true;
    }

    if ((event & provisioning_button_release_event) != 0u)
    {
        provisioning_button_irq_is_pressed = false;
        provisioning_button_irq_released = true;
    }
}

/*******************************************************************************
* Function Name: classify_wifi_connect_failure
********************************************************************************
* Summary:
*  Maps low-level Wi-Fi connect result code to a higher-level failure reason.
*******************************************************************************/
static wifi_fail_reason_t classify_wifi_connect_failure(cy_rslt_t result_code)
{
    if (result_code == CY_RSLT_SUCCESS)
    {
        return WIFI_FAIL_REASON_NONE;
    }

    if ((uint32_t)result_code == WIFI_JOIN_AUTH_FAILURE_RSLT)
    {
        return WIFI_FAIL_REASON_AUTH_CREDENTIALS;
    }

    /* Fallback bucket when AP may be unavailable / weak signal / transient issue. */
    return WIFI_FAIL_REASON_AP_UNREACHABLE;
}

/*******************************************************************************
* Function Name: wifi_fail_reason_to_text
********************************************************************************
* Summary:
*  Returns a short, printable label for failure reason telemetry/logging.
*******************************************************************************/
static const char* wifi_fail_reason_to_text(wifi_fail_reason_t reason)
{
    switch (reason)
    {
        case WIFI_FAIL_REASON_NONE:
            return "none";
        case WIFI_FAIL_REASON_BUTTON_ABORT:
            return "button_abort";
        case WIFI_FAIL_REASON_AUTH_CREDENTIALS:
            return "auth_or_credentials";
        case WIFI_FAIL_REASON_CONNECT_TIMEOUT:
            return "connect_timeout";
        case WIFI_FAIL_REASON_AP_UNREACHABLE:
            return "ap_unreachable_or_transient";
        default:
            return "unknown";
    }
}

/*******************************************************************************
* Function Name: format_ipv4_address
********************************************************************************
* Summary:
*  Converts a packed IPv4 address into dotted-decimal text.
*******************************************************************************/
static void format_ipv4_address(uint32_t ip_addr, char *buffer, size_t buffer_len)
{
    snprintf(buffer, buffer_len, "%u.%u.%u.%u",
             (unsigned char)((ip_addr >> 0) & 0xff),
             (unsigned char)((ip_addr >> 8) & 0xff),
             (unsigned char)((ip_addr >> 16) & 0xff),
             (unsigned char)((ip_addr >> 24) & 0xff));
}

/*******************************************************************************
* Function Name: start_sta_mdns_service
********************************************************************************
* Summary:
*  Starts mDNS responder for STA interface so the web UI is reachable as:
*      http://<STA_MDNS_HOSTNAME>.local/
*  Called only after STA mode is up and STA HTTP server is running.
*
* Return:
*  cy_rslt_t: CY_RSLT_SUCCESS on success, error otherwise.
*******************************************************************************/
static cy_rslt_t start_sta_mdns_service(void)
{
#if LWIP_MDNS_RESPONDER
    cy_wcm_ip_address_t sta_ip;
    struct netif *sta_netif =
        (struct netif *)cy_network_get_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0u);

    if (sta_netif == NULL)
    {
        ERR_INFO(("mDNS: STA netif handle is NULL.\r\n"));
        return CY_RSLT_TYPE_ERROR;
    }

    if (cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &sta_ip) != CY_RSLT_SUCCESS)
    {
        ERR_INFO(("mDNS: STA IP address not ready yet.\r\n"));
        return CY_RSLT_TYPE_ERROR;
    }

    LOCK_TCPIP_CORE();

    if (!sta_mdns_initialized)
    {
        mdns_resp_init();
        sta_mdns_initialized = true;
    }

    if (sta_mdns_active)
    {
        (void)mdns_resp_remove_netif(sta_netif);
        sta_mdns_http_service_slot = -1;
        sta_mdns_active = false;
    }

    err_t mdns_result = mdns_resp_add_netif(sta_netif, STA_MDNS_HOSTNAME, STA_MDNS_HOST_TTL_SECONDS);
    if (mdns_result != ERR_OK)
    {
        UNLOCK_TCPIP_CORE();
        ERR_INFO(("mDNS: mdns_resp_add_netif failed (%d).\r\n", (int)mdns_result));
        return CY_RSLT_TYPE_ERROR;
    }

    sta_mdns_http_service_slot = mdns_resp_add_service(sta_netif,
                                                       STA_MDNS_HTTP_SERVICE_NAME,
                                                       "_http",
                                                       DNSSD_PROTO_TCP,
                                                       HTTP_PORT,
                                                       STA_MDNS_SERVICE_TTL_SECONDS,
                                                       NULL,
                                                       NULL);

    if (sta_mdns_http_service_slot < 0)
    {
        (void)mdns_resp_remove_netif(sta_netif);
        sta_mdns_http_service_slot = -1;
        sta_mdns_active = false;
        UNLOCK_TCPIP_CORE();
        ERR_INFO(("mDNS: HTTP service registration failed (slot=%d).\r\n",
                  (int)sta_mdns_http_service_slot));
        return CY_RSLT_TYPE_ERROR;
    }

    mdns_resp_announce(sta_netif);
    sta_mdns_active = true;
    sta_mdns_retry_elapsed_ms = 0u;
    UNLOCK_TCPIP_CORE();

    APP_INFO(("mDNS started: http://%s.local/\r\n", STA_MDNS_HOSTNAME));
    return CY_RSLT_SUCCESS;
#else
    return CY_RSLT_SUCCESS;
#endif
}

/*******************************************************************************
* Function Name: stop_sta_mdns_service
********************************************************************************
* Summary:
*  Stops mDNS responder binding for STA interface.
*******************************************************************************/
static void stop_sta_mdns_service(void)
{
#if LWIP_MDNS_RESPONDER
    if (!sta_mdns_active)
    {
        sta_mdns_retry_elapsed_ms = 0u;
        return;
    }

    struct netif *sta_netif =
        (struct netif *)cy_network_get_nw_interface(CY_NETWORK_WIFI_STA_INTERFACE, 0u);

    if (sta_netif == NULL)
    {
        sta_mdns_http_service_slot = -1;
        sta_mdns_active = false;
        return;
    }

    LOCK_TCPIP_CORE();
    (void)mdns_resp_remove_netif(sta_netif);
    sta_mdns_http_service_slot = -1;
    sta_mdns_active = false;
    UNLOCK_TCPIP_CORE();
    sta_mdns_retry_elapsed_ms = 0u;
#endif
}

/*******************************************************************************
* Function Name: service_sta_mdns_retry
********************************************************************************
* Summary:
*  Retries mDNS startup in STA mode when a previous attempt failed because the
*  interface or IP address was not ready yet.
*******************************************************************************/
static void service_sta_mdns_retry(uint32_t elapsed_ms)
{
#if LWIP_MDNS_RESPONDER
    if (sta_mdns_active || (reconfiguration_request != SERVER_RECONFIGURED))
    {
        sta_mdns_retry_elapsed_ms = 0u;
        return;
    }

    if (!cy_wcm_is_connected_to_ap())
    {
        sta_mdns_retry_elapsed_ms = 0u;
        return;
    }

    sta_mdns_retry_elapsed_ms += elapsed_ms;
    if (sta_mdns_retry_elapsed_ms < 1000u)
    {
        return;
    }

    sta_mdns_retry_elapsed_ms = 0u;
    if (start_sta_mdns_service() != CY_RSLT_SUCCESS)
    {
        ERR_INFO(("mDNS retry failed; will retry again.\r\n"));
    }
#else
    (void)elapsed_ms;
#endif
}

/*******************************************************************************
 * Function Name: process_sse_handler
 *******************************************************************************
 * Summary:
 *  Handler for enabling server sent events
 *
 * Parameters:
 *  url_path - Pointer to the HTTP URL path.
 *  url_parameters - Pointer to the HTTP URL query string.
 *  stream - Pointer to the HTTP response stream.
 *  arg - Pointer to the argument passed during HTTP resource registration.
 *  http_message_body - Pointer to the HTTP data from the client.
 *
 * Return:
 *  int32_t - Returns HTTP_REQUEST_HANDLE_SUCCESS if the request from the client
 *  was handled successfully. Otherwise, it returns HTTP_REQUEST_HANDLE_ERROR.
 *
 *******************************************************************************/
int32_t process_sse_handler( const char* url_path, const char* url_parameters,
                                   cy_http_response_stream_t* stream, void* arg,
                                   cy_http_message_body_t* http_message_body )
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Assign the incoming stream to http_event_stream pointer */
    http_event_stream = stream;

    /* Enable chunked transfer encoding on the HTTP stream */
    result = cy_http_server_response_stream_enable_chunked_transfer( http_event_stream );
    PRINT_AND_ASSERT(result, "HTTP server event failed to enable chunked transfer\r\n");

    result = cy_http_server_response_stream_write_header( http_event_stream, CY_HTTP_200_TYPE,
                                                CHUNKED_CONTENT_LENGTH, CY_HTTP_CACHE_DISABLED,
                                                MIME_TYPE_TEXT_EVENT_STREAM );
    PRINT_AND_ASSERT(result, "HTTP server event failed to write stream header\r\n");

    return result;
}

/*******************************************************************************
 * Function Name: softap_resource_handler
 *******************************************************************************
 * Summary:
 *  Handles HTTP GET, POST, and PUT requests from the client.
 *  HTTP GET sends the HTTP startup webpage as a response to the client.
 *  HTTP POST extracts the credentials from the HTTP data from the client 
 *  and tries to connect to the AP.
 *  HTTP PUT sends an error message as a response to the client if the resource
 *  registration is unsuccessful.
 *
 * Parameters:
 *  url_path - Pointer to the HTTP URL path.
 *  url_parameters - Pointer to the HTTP URL query string.
 *  stream - Pointer to the HTTP response stream.
 *  arg - Pointer to the argument passed during HTTP resource registration.
 *  http_message_body - Pointer to the HTTP data from the client.
 *
 * Return:
 *  int32_t - Returns HTTP_REQUEST_HANDLE_SUCCESS if the request from the client
 *  was handled successfully. Otherwise, it returns HTTP_REQUEST_HANDLE_ERROR.
 *
 *******************************************************************************/
int32_t softap_resource_handler(const char *url_path,
                                 const char *url_parameters,
                                 cy_http_response_stream_t *stream,
                                 void *arg,
                                 cy_http_message_body_t *http_message_body)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int32_t status = HTTP_REQUEST_HANDLE_SUCCESS;

    switch (http_message_body->request_type)
    {
    case CY_HTTP_REQUEST_GET:

        /* If device is not configured send the initial page */
        if(!device_configured)
        {
            /* The start up page of the HTTP client will be sent as an initial response
             * to the GET request.
             */
            result = cy_http_server_response_stream_write_payload(stream, HTTP_SOFTAP_STARTUP_WEBPAGE, sizeof(HTTP_SOFTAP_STARTUP_WEBPAGE) - 1);
            if (CY_RSLT_SUCCESS != result)
            {
                ERR_INFO(("Failed to send the HTTP GET response.\r\n"));
            }
        }
        else
        {
            /* Send the data of the device */
            result = cy_http_server_response_stream_write_payload(stream, SOFTAP_DEVICE_DATA, sizeof(SOFTAP_DEVICE_DATA) - 1);
            if (CY_RSLT_SUCCESS != result)
            {
                ERR_INFO(("Failed to send the HTTP GET response.\n"));
            }
        }
        break;

    case CY_HTTP_REQUEST_POST:

        if(!device_configured)
        {
            /* The device tries to connect to the AP using the credentials sent via HTTP
             * webpage.
             */
            result = wifi_extract_credentials(http_message_body->data, http_message_body->data_length,stream);
        }
        else
        {
            size_t cmd_len = (size_t)http_message_body->data_length;
            const uint8_t *cmd_data = http_message_body->data;

            if ((cmd_len == strlen(LIN_HTTP_CMD_CALIBRATE)) &&
                (memcmp(cmd_data, LIN_HTTP_CMD_CALIBRATE, cmd_len) == 0))
            {
                lin_actuator_request_calibration();
            }
            else if ((cmd_len == strlen(LIN_HTTP_CMD_OPEN)) &&
                     (memcmp(cmd_data, LIN_HTTP_CMD_OPEN, cmd_len) == 0))
            {
                lin_actuator_request_open();
            }
            else if ((cmd_len == strlen(LIN_HTTP_CMD_CLOSE)) &&
                     (memcmp(cmd_data, LIN_HTTP_CMD_CLOSE, cmd_len) == 0))
            {
                lin_actuator_request_close();
            }
            else
            {
                ERR_INFO(("Unknown actuator command received from HTTP client.\r\n"));
            }

            /* Send the HTTP response. */
            result = cy_http_server_response_stream_write_payload(stream, HTTP_HEADER_204, sizeof(HTTP_HEADER_204) - 1);
            if (CY_RSLT_SUCCESS != result)
            {
                ERR_INFO(("Failed to send the HTTP POST response.\n"));
            }
        }
    break;

    default:
        ERR_INFO(("Received invalid HTTP request method. Supported HTTP methods are GET, POST, and PUT.\n"));
        
        break;

    }

    if (CY_RSLT_SUCCESS != result)
    {
        status = HTTP_REQUEST_HANDLE_ERROR;
    }

    return status;
}

/*******************************************************************************
 * Function Name: wifi_resource_handler
 *******************************************************************************
 * Summary:
 *  Handles HTTP GET, POST, and PUT requests from the client.
 *  HTTP GET performs scan for available networks(APs) and sends the list of 
 *  available networks as a response to the client.
 *  HTTP POST extracts the credentials from the HTTP data from the client 
 *  and tries to connect to the AP.
 *
 * Parameters:
 *  url_path - Pointer to the HTTP URL path.
 *  url_parameters - Pointer to the HTTP URL query string.
 *  stream - Pointer to the HTTP response stream.
 *  arg - Pointer to the argument passed during HTTP resource registration.
 *  http_message_body - Pointer to the HTTP data from the client.
 *
 * Return:
 *  int32_t - Returns HTTP_REQUEST_HANDLE_SUCCESS if the request from the client
 *  was handled successfully. Otherwise, it returns HTTP_REQUEST_HANDLE_ERROR.
 *
 *******************************************************************************/
static int32_t wifi_resource_handler(const char *url_path,
                                     const char *url_parameters,
                                     cy_http_response_stream_t *stream,
                                     void *arg,
                                     cy_http_message_body_t *http_message_body)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int32_t status = HTTP_REQUEST_HANDLE_SUCCESS;
    
    switch (http_message_body->request_type)
    {
    case CY_HTTP_REQUEST_GET:

        /* Scan for the available networks in response to the HTTP GET request. */
        scan_for_available_aps(stream);
        
        break;

    case CY_HTTP_REQUEST_POST:

        /* The device tries to connect to the AP using the credentials sent via HTTP
         * webpage.
         */
        result = cy_http_server_response_stream_write_payload(stream, HTTP_DEVICE_DATA_REDIRECT_WEBPAGE, sizeof(HTTP_DEVICE_DATA_REDIRECT_WEBPAGE));
        if (CY_RSLT_SUCCESS != result)
        {
            ERR_INFO(("Failed to send the HTTP POST response.\n"));
        }

        /* Set device configured flag to true */
        device_configured = true;
        reconfiguration_request = SERVER_RECONFIGURE_REQUESTED;
        cy_wcm_stop_ap();
        break;

    default:
        ERR_INFO(("Wi-Fi Scan: Received invalid HTTP request method. Supported HTTP methods are GET, POST, and PUT.\n"));
        
        break;

    }

    if (CY_RSLT_SUCCESS != result)
    {
        status = HTTP_REQUEST_HANDLE_ERROR;
    }

    return status;
}

/*******************************************************************************
 * Function Name: scan_callback
 *******************************************************************************
 * Summary: The callback function which accumulates the scan results. After
 * completing the scan, it updates scan_complete_flag to indicate end of scan.
 *
 * Parameters:
 *  cy_wcm_scan_result_t *result_ptr: Pointer to the scan result
 *  void *user_data: User data.
 *  cy_wcm_scan_status_t status: Status of scan completion.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void scan_callback(cy_wcm_scan_result_t *result_ptr, void *user_data, cy_wcm_scan_status_t status)
{
    static uint32_t len = 0;

    (void)user_data;

    if ((status == CY_WCM_SCAN_INCOMPLETE) && (result_ptr != NULL))
    {
        size_t ssid_len = strnlen((const char *)result_ptr->SSID, WIFI_SSID_LEN);

        if ((ssid_len != 0u) && (len < (sizeof(ssid_buff) - 1u)))
        {
            size_t remaining = (sizeof(ssid_buff) - 1u) - len;

            if (ssid_len > remaining)
            {
                ssid_len = remaining;
            }

            memcpy(&ssid_buff[len], result_ptr->SSID, ssid_len);
            len += (uint32_t)ssid_len;

            if (len < (sizeof(ssid_buff) - 1u))
            {
                ssid_buff[len++] = '\n';
            }

            ssid_buff[len] = '\0';
        }
    }

    if ((CY_WCM_SCAN_COMPLETE == status))
    {
        /* Flag to notify that scan has completed.*/
        if (len >= sizeof(ssid_buff))
        {
            len = sizeof(ssid_buff) - 1u;
        }
        ssid_buff[len] = '\0';
        scan_complete_flag = true;
        len = 0;
    }
}

/*******************************************************************************
 * Function Name: scan_for_available_aps
 *******************************************************************************
 * Summary: This function scans for available APs and prints the scan result to 
 * the webpage once the scan is complete.
 *
 *
 * Parameters:
 *  cy_http_response_stream_t *url_stream : HTTP stream on which data was received.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void scan_for_available_aps(cy_http_response_stream_t *url_stream)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    size_t response_len = 0u;

    memset(ssid_buff, 0, sizeof(ssid_buff));
    memset(http_scan_response, 0, sizeof(http_scan_response));
    scan_complete_flag = false;

    result = cy_http_server_response_stream_write_payload(url_stream, WIFI_SCAN_IN_PROGRESS, sizeof(WIFI_SCAN_IN_PROGRESS));
    PRINT_AND_ASSERT(result, "Failed to send the HTTP POST response.\n");

    display_status_show_scanning();

    result = cy_wcm_start_scan(scan_callback, NULL, NULL);
    PRINT_AND_ASSERT(result, "cy_wcm_start_scan failed.\n");
    
    /* Waiting for the scan to be completed */
    while (!scan_complete_flag)
    {
        vTaskDelay(pdMS_TO_TICKS(SCAN_DELAY_MS));
    }

    /* Print the scan result in webpage.*/
    response_len = append_text(http_scan_response, sizeof(http_scan_response), response_len,
                               SOFTAP_SCAN_START_RESPONSE);
    response_len = append_text(http_scan_response, sizeof(http_scan_response), response_len,
                               ssid_buff);
    response_len = append_text(http_scan_response, sizeof(http_scan_response), response_len,
                               SOFTAP_SCAN_INTERMEDIATE_RESPONSE);
    response_len = append_text(http_scan_response, sizeof(http_scan_response), response_len,
                               SOFTAP_SCAN_END_RESPONSE);

    result = cy_http_server_response_stream_write_payload(url_stream, http_scan_response, response_len);
    if (CY_RSLT_SUCCESS != result)
    {
        ERR_INFO(("Failed to write HTTP response\r\n"));
    }

}

/********************************************************************************
 * Function Name: wifi_extract_credentials
 ********************************************************************************
 * Summary:
 *  The function extracts the credentials entered via HTTP webpage. Switches to STA 
 *  mode then connects to the same credentials.
 *
 * Parameters:
 *  const uint8_t* data : The HTTP data that contains ssid and password that is
 *  entered from the HTTP webpage.
*   uint32_t data_len : The length of the HTTP response.
 *
 * Return:
 *  void
 *
 *******************************************************************************/
cy_rslt_t wifi_extract_credentials(const uint8_t *data, uint32_t data_len, cy_http_response_stream_t *stream)
{
    int8_t ssid_buff_index, buff_index = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    size_t response_len = 0u;

    /* Reset parsed credential buffers so reprovisioning cannot keep stale tail bytes. */
    memset(wifi_ssid, 0, sizeof(wifi_ssid));
    memset(wifi_pwd, 0, sizeof(wifi_pwd));
    memset(buffer, 0, sizeof(buffer));
    memset(http_wifi_connect_response, 0, sizeof(http_wifi_connect_response));

    /*decode the url encoded data using the function url_decode()*/
    url_decode(buffer, data);

    if (!strncmp("SSID", buffer, 4))
    {
        /* Extract SSID and Password - skip to SSID*/
        while ((buffer[buff_index++] != EQUALS_OPERATOR_ASCII_VALUE))
            ;

        ssid_buff_index = 0;
        /*skip '&' */
        while ((buffer[buff_index] != AMPERSAND_OPERATOR_ASCII_VALUE))
        {
            if (ssid_buff_index >= (WIFI_SSID_LEN - 1))
            {
                break;
            }
            wifi_ssid[ssid_buff_index++] = buffer[buff_index++];
        }
        wifi_ssid[ssid_buff_index] = '\0';

        buff_index++;
        /* skip to Password */
        while ((buffer[buff_index++] != EQUALS_OPERATOR_ASCII_VALUE ))
            ;

        ssid_buff_index = 0;
        while ((buff_index < data_len))
        {
            if (buffer[buff_index] == AMPERSAND_OPERATOR_ASCII_VALUE)
                break;
            if (ssid_buff_index >= (WIFI_PWD_LEN - 1))
            {
                break;
            }

            wifi_pwd[ssid_buff_index++] = buffer[buff_index++];
        }
        wifi_pwd[ssid_buff_index] = '\0';
    }
    result = cy_http_server_response_stream_write_payload(stream, WIFI_CONNECT_IN_PROGRESS, sizeof(WIFI_CONNECT_IN_PROGRESS));
    if (CY_RSLT_SUCCESS != result)
    {
        ERR_INFO(("Failed to send the HTTP POST response.\n"));
    }

    display_status_show_connecting((const char *)wifi_ssid, 1u, MAX_WIFI_RETRY_COUNT);

    APP_INFO(("Exiting provisioning/AP mode.\r\n"));
    result = start_sta_mode();
    if (CY_RSLT_SUCCESS != result)
    {
        if (provisioning_button_force_mode_requested)
        {
            APP_INFO(("Wi-Fi connect canceled by button request; staying in provisioning/AP mode.\r\n"));
            provisioning_button_force_mode_requested = false;
        }
        else if (last_wifi_fail_reason == WIFI_FAIL_REASON_AUTH_CREDENTIALS)
        {
            ERR_INFO(("Wi-Fi authentication failed. Check SSID/password and retry provisioning.\r\n"));
        }
        else
        {
            ERR_INFO(("Wi-Fi connection failure (%s) and fallback to provisioning/AP mode.\r\n",
                      wifi_fail_reason_to_text(last_wifi_fail_reason)));
        }
        display_status_show_connect_failure((const char *)wifi_ssid,
                                            wifi_fail_reason_to_text(last_wifi_fail_reason));
        response_len = append_text(http_wifi_connect_response, sizeof(http_wifi_connect_response),
                                   response_len, WIFI_CONNECT_RESPONSE_START);
        response_len = append_text(http_wifi_connect_response, sizeof(http_wifi_connect_response),
                                   response_len, WIFI_CONNECT_FAIL_RESPONSE_END);
        result = cy_http_server_response_stream_write_payload(stream, http_wifi_connect_response, response_len);
        if (CY_RSLT_SUCCESS != result)
        {
            ERR_INFO(("Failed to send the HTTP POST response.\n"));
        }
    }
    else
    {
        response_len = append_text(http_wifi_connect_response, sizeof(http_wifi_connect_response),
                                   response_len, WIFI_CONNECT_RESPONSE_START);
        response_len = append_text(http_wifi_connect_response, sizeof(http_wifi_connect_response),
                                   response_len, WIFI_CONNECT_SUCCESS_RESPONSE_END);
        result = cy_http_server_response_stream_write_payload(stream, http_wifi_connect_response, response_len);
        if (CY_RSLT_SUCCESS != result)
        {
            ERR_INFO(("Failed to send the HTTP POST response.\n"));
        }

        /**********************************************************************
         * Phase 1: Store Wi-Fi network into internal flash
         **********************************************************************
         * Summary:
         *  Store the SSID/PWD so future boots can skip provisioning.
         *  If storage is full, the store will evict the least recently used.
         *********************************************************************/
        cy_rslt_t save_r = wifi_store_save_network((const char*)wifi_ssid, (const char*)wifi_pwd);
        if (save_r == CY_RSLT_SUCCESS)
        {
            APP_INFO(("Stored Wi-Fi network in internal flash.\r\n"));
        }
        else
        {
            ERR_INFO(("Failed to store Wi-Fi network (0x%08lx).\r\n", (unsigned long)save_r));
        }
    }
    return result;
}

/********************************************************************************
 * Function Name: start_ap_mode
 ********************************************************************************
 * Summary:
 *  The function configures device in Concurrent AP + STA mode and initialises
 *  a SoftAP with the given credentials (SOFTAP_SSID, SOFTAP_PASSWORD and  security
 *  CY_WCM_SOFTAP_PASSWORD_WPA2_AES_PSK). 
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t: Returns CY_RSLT_SUCCESS if the SoftAP is started successfully,
 *  a WCM error code otherwise.
 *
 *******************************************************************************/
cy_rslt_t start_ap_mode()
{
    cy_rslt_t result;
    cy_wcm_ap_config_t ap_conf;
    cy_wcm_ip_address_t ipv4_addr;

    memset(&ap_conf, 0, sizeof(cy_wcm_ap_config_t));
    memset(&ipv4_addr, 0, sizeof(cy_wcm_ip_address_t));

    ap_conf.channel = 1;
    memcpy(ap_conf.ap_credentials.SSID, SOFTAP_SSID, strlen(SOFTAP_SSID) + 1);
    memcpy(ap_conf.ap_credentials.password, SOFTAP_PASSWORD, strlen(SOFTAP_PASSWORD) + 1);
    ap_conf.ap_credentials.security = SOFTAP_SECURITY_TYPE;
    ap_conf.ip_settings.ip_address = ap_sta_mode_ip_settings.ip_address;
    ap_conf.ip_settings.netmask = ap_sta_mode_ip_settings.netmask;
    ap_conf.ip_settings.gateway = ap_sta_mode_ip_settings.gateway;

    result = cy_wcm_start_ap(&ap_conf);
    PRINT_AND_ASSERT(result, "cy_wcm_start_ap failed...! \n");

    /* Get IPV4 address for AP */
    result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_AP, &ipv4_addr);
    PRINT_AND_ASSERT(result, "cy_wcm_get_ip_addr failed...! \n");

    char ap_ip_text[DISPLAY_BUFFER_LENGTH];
    format_ipv4_address(ipv4_addr.ip.v4, ap_ip_text, sizeof(ap_ip_text));
    display_status_show_provisioning(SOFTAP_SSID, SOFTAP_PASSWORD, ap_ip_text, NULL);

    return result;
}

/*******************************************************************************
 * Function Name: start_sta_mode
 *******************************************************************************
 * Summary:
 *  The function attempts to connect to Wi-Fi until a connection is made or
 *  MAX_WIFI_RETRY_COUNT attempts have been made.
 * 
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t: Returns CY_RSLT_SUCCESS if the HTTP server is configured
 *  successfully, otherwise, it returns CY_RSLT_TYPE_ERROR.
 *
 *******************************************************************************/
cy_rslt_t start_sta_mode()
{
    cy_rslt_t result = CY_RSLT_TYPE_ERROR;
    cy_wcm_connect_params_t connect_param;
    cy_wcm_ip_address_t ip_address;
    bool wifi_conct_stat = false;
    uint32_t retry_delay_ms = WIFI_CONN_RETRY_INTERVAL_MSEC;

    APP_INFO(("Entering STA mode.\r\n"));
    provisioning_button_force_mode_requested = false;
    last_wifi_fail_reason = WIFI_FAIL_REASON_NONE;

    /*Disconnect from the currently connected AP if any*/
    wifi_conct_stat = cy_wcm_is_connected_to_ap();
    if(wifi_conct_stat)
    {
        cy_wcm_disconnect_ap();
    }

    memset(&connect_param, 0, sizeof(cy_wcm_connect_params_t));
    memset(&ip_address, 0, sizeof(cy_wcm_ip_address_t));
   
    memcpy(connect_param.ap_credentials.SSID, wifi_ssid, sizeof(wifi_ssid));
    memcpy(connect_param.ap_credentials.password, wifi_pwd, sizeof(wifi_pwd));
    connect_param.ap_credentials.security = CY_WCM_SECURITY_WPA2_AES_PSK;

    /* Attempt to connect to Wi-Fi until a connection is made or
     * MAX_WIFI_RETRY_COUNT attempts have been made.
     */
    for (uint32_t conn_retries = 0; conn_retries < MAX_WIFI_RETRY_COUNT; conn_retries++)
    {
        display_status_show_connecting((const char *)connect_param.ap_credentials.SSID,
                                       conn_retries + 1u, MAX_WIFI_RETRY_COUNT);

        /* Allow provisioning button to interrupt STA connect attempts at any point. */
        handle_runtime_force_provisioning(SERVER_LOOP_PERIOD_MS);
        if (provisioning_button_force_mode_requested)
        {
            APP_INFO(("Wi-Fi connect aborted by button request; entering provisioning/AP mode.\r\n"));
            last_wifi_fail_reason = WIFI_FAIL_REASON_BUTTON_ABORT;
            return CY_RSLT_TYPE_ERROR;
        }

        result = cy_wcm_connect_ap(&connect_param, &ip_address);
        if (result == CY_RSLT_SUCCESS)
        {
            APP_INFO(("Successful Wi-Fi connection: '%s'.\r\n", connect_param.ap_credentials.SSID));
            last_wifi_fail_reason = WIFI_FAIL_REASON_NONE;
            char sta_ip_text[DISPLAY_BUFFER_LENGTH];
            format_ipv4_address(ip_address.ip.v4, sta_ip_text, sizeof(sta_ip_text));
            display_status_show_connect_success((const char *)connect_param.ap_credentials.SSID,
                                                sta_ip_text, STA_MDNS_HOSTNAME ".local");
            break;
        }
        last_wifi_fail_reason = classify_wifi_connect_failure(result);
        display_status_show_connect_failure((const char *)connect_param.ap_credentials.SSID,
                                            wifi_fail_reason_to_text(last_wifi_fail_reason));
        ERR_INFO(("Wi-Fi connection attempt %lu/%lu failed (error %ld, reason=%s). Retrying in %lu ms...\r\n",
                  (unsigned long)(conn_retries + 1u),
                  (unsigned long)MAX_WIFI_RETRY_COUNT,
                  (long)result,
                  wifi_fail_reason_to_text(last_wifi_fail_reason),
                  (unsigned long)retry_delay_ms));

        /* Invalid credentials typically do not recover with retries; fail over quickly. */
        if (last_wifi_fail_reason == WIFI_FAIL_REASON_AUTH_CREDENTIALS)
        {
            break;
        }

        uint32_t wait_ms = 0;
        while (wait_ms < retry_delay_ms)
        {
            uint32_t slice_ms = SERVER_LOOP_PERIOD_MS;
            if ((retry_delay_ms - wait_ms) < slice_ms)
            {
                slice_ms = retry_delay_ms - wait_ms;
            }

            vTaskDelay(pdMS_TO_TICKS(slice_ms));
            wait_ms += slice_ms;

            handle_runtime_force_provisioning(slice_ms);
            if (provisioning_button_force_mode_requested)
            {
                APP_INFO(("Wi-Fi connect aborted by button request; entering provisioning/AP mode.\r\n"));
                last_wifi_fail_reason = WIFI_FAIL_REASON_BUTTON_ABORT;
                return CY_RSLT_TYPE_ERROR;
            }
        }

        if (retry_delay_ms < WIFI_CONN_BACKOFF_MAX_MSEC)
        {
            retry_delay_ms <<= 1u;
            if (retry_delay_ms > WIFI_CONN_BACKOFF_MAX_MSEC)
            {
                retry_delay_ms = WIFI_CONN_BACKOFF_MAX_MSEC;
            }
        }
    }

    return result;
}

/*******************************************************************************
 * Function Name: configure_http_server
 *******************************************************************************
 * Summary:
 *  The function registers a softap_resource_handler to handle HTTP requests 
 *  received by http_ap_server. 
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t: Returns CY_RSLT_SUCCESS if the HTTP server is configured
 *  successfully, otherwise, it returns CY_RSLT_TYPE_ERROR.
 *
 *******************************************************************************/
cy_rslt_t configure_http_server(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_wcm_ip_address_t ip_addr;

    /* Holds the response handler for HTTP GET and POST request from the client. */
    cy_resource_dynamic_data_t http_get_post_resource;

    /* IP address of SoftAp. */
    result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_AP, &ip_addr);
    PRINT_AND_ASSERT(result, "cy_wcm_get_ip_addr failed for creating HTTP server...! \n");

    http_server_ip_address.ip_address.ip.v4 = ip_addr.ip.v4;
    http_server_ip_address.ip_address.version = CY_SOCKET_IP_VER_V4;

    /* Add IP address information to network interface object. */
    nw_interface.object = (void *)&http_server_ip_address;
    nw_interface.type = CY_NW_INF_TYPE_WIFI;

    /* Initialize secure socket library. */
    result = cy_http_server_network_init();

    /* Allocate memory needed for secure HTTP server. */
    result = cy_http_server_create(&nw_interface, HTTP_PORT, MAX_SOCKETS, NULL, &http_ap_server);
    PRINT_AND_ASSERT(result, "Failed to allocate memory for the HTTP server.\n");

    /* Configure dynamic resource handler. */
    http_get_post_resource.resource_handler = softap_resource_handler;
    http_get_post_resource.arg = NULL;

    /* Register all the resources with the secure HTTP server. */
    result = cy_http_server_register_resource(http_ap_server,
                                              (uint8_t *)"/",
                                              (uint8_t *)"text/html",
                                              CY_DYNAMIC_URL_CONTENT,
                                              &http_get_post_resource);
    PRINT_AND_ASSERT(result, "Failed to register a resource.\n");

    /* Configure dynamic resource handler. */
    http_wifi_resource.resource_handler = wifi_resource_handler;
    http_wifi_resource.arg = NULL;

    /* Register all the resources with the secure HTTP server. */
    result = cy_http_server_register_resource(http_ap_server,
                                              (uint8_t *)"/wifi_scan_form",
                                              (uint8_t *)"text/html",
                                              CY_DYNAMIC_URL_CONTENT,
                                              &http_wifi_resource);
    PRINT_AND_ASSERT(result, "Failed to register a resource.\n");

    return result;
}

/*******************************************************************************
 * Function Name: configure_http_server_sta_only
 *******************************************************************************
 * Summary:
 *  Creates and starts the HTTP server directly in STA mode.
 *  Used when the device boots and successfully connects to a saved network.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t: Returns CY_RSLT_SUCCESS if the HTTP server is configured
 *  successfully, otherwise returns an error code.
 *
 *******************************************************************************/
cy_rslt_t configure_http_server_sta_only(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_wcm_ip_address_t ip_addr;

    /* Holds the response handler for HTTP GET and POST request from the client. */
    cy_resource_dynamic_data_t http_get_post_resource;

    /* Holds the response handler for dynamic SSE resource. */
    cy_resource_dynamic_data_t dynamic_sse_resource;

    /* Get STA IP address */
    result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
    PRINT_AND_ASSERT(result, "cy_wcm_get_ip_addr failed for STA HTTP server.\n");

    http_server_ip_address.ip_address.ip.v4 = ip_addr.ip.v4;
    http_server_ip_address.ip_address.version = CY_SOCKET_IP_VER_V4;

    /* Add IP address information to network interface object. */
    nw_interface.object = (void *)&http_server_ip_address;
    nw_interface.type = CY_NW_INF_TYPE_WIFI;

    /* Initialize secure socket library. */
    result = cy_http_server_network_init();
    PRINT_AND_ASSERT(result, "cy_http_server_network_init failed.\n");

    /* Create HTTP server instance for STA */
    result = cy_http_server_create(&nw_interface, HTTP_PORT, MAX_SOCKETS, NULL, &http_sta_server);
    PRINT_AND_ASSERT(result, "cy_http_server_create(STA) failed.\n");

    /* Register SSE resource */
    dynamic_sse_resource.resource_handler = process_sse_handler;
    dynamic_sse_resource.arg = NULL;
    result = cy_http_server_register_resource(http_sta_server,
                                              (uint8_t*)"/events",
                                              (uint8_t*)"text/event-stream",
                                              CY_RAW_DYNAMIC_URL_CONTENT,
                                              &dynamic_sse_resource);
    PRINT_AND_ASSERT(result, "Failed to register /events resource.\n");

    /* Root resource (same handler used in template) */
    http_get_post_resource.resource_handler = softap_resource_handler;
    http_get_post_resource.arg = NULL;
    result = cy_http_server_register_resource(http_sta_server,
                                              (uint8_t*)"/",
                                              (uint8_t*)"text/html",
                                              CY_DYNAMIC_URL_CONTENT,
                                              &http_get_post_resource);
    PRINT_AND_ASSERT(result, "Failed to register / resource.\n");

    /* Start STA server */
    result = cy_http_server_start(http_sta_server);
    PRINT_AND_ASSERT(result, "Failed to start STA HTTP server.\n");

    /* Advertise STA web UI hostname on LAN via mDNS. */
    cy_rslt_t mdns_result = start_sta_mdns_service();
    if (mdns_result != CY_RSLT_SUCCESS)
    {
        ERR_INFO(("mDNS start failed in STA-only mode.\r\n"));
    }

    return result;
}

/*******************************************************************************
 * Function Name: reconfigure_http_server
 *******************************************************************************
 * Summary:
 * The function deletes the existing HTTP server instance (http_ap_server), and 
 * starts a new HTTP server instance (http_sta_server). After registering 
 * dynamic URL handler (process_sse_handler and http_get_post_resource) to handle 
 * the HTTP GET, POST, and PUT requests.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t: Returns CY_RSLT_SUCCESS if the HTTP server is configured
 *  successfully, otherwise, it returns CY_RSLT_TYPE_ERROR.
 *
 *******************************************************************************/
cy_rslt_t reconfigure_http_server(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_wcm_ip_address_t ip_addr;

    /* Holds the response handler for HTTP GET and POST request from the client. */
    cy_resource_dynamic_data_t http_get_post_resource;

    /* Holds the response handler for dynamic SSE resource. */
    cy_resource_dynamic_data_t dynamic_sse_resource;

    APP_INFO(("Exiting provisioning/AP mode.\r\n"));
    APP_INFO(("Entering STA mode.\r\n"));

    /* Restart HTTP server using the new ip address. */
    result = cy_http_server_stop( http_ap_server );
    PRINT_AND_ASSERT(result, "Failed to stop HTTP server.\n");

    /* Delete the HTTP server object */
    result = cy_http_server_delete( http_ap_server );
    PRINT_AND_ASSERT(result, "Failed to delete HTTP server.\n");

    /* Deinitialize the network socket */
    result = cy_http_server_network_deinit();
    PRINT_AND_ASSERT(result, "Failed to deinit server.\n");

    /* IP address of SoftAp. */
    result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
    PRINT_AND_ASSERT(result, "cy_wcm_get_ip_addr failed for creating HTTP server...! \n");

    http_server_ip_address.ip_address.ip.v4 = ip_addr.ip.v4;
    http_server_ip_address.ip_address.version = CY_SOCKET_IP_VER_V4;

    /* Add IP address information to network interface object. */
    nw_interface.object = (void *)&http_server_ip_address;
    nw_interface.type = CY_NW_INF_TYPE_WIFI;

    /* Initialize secure socket library. */
    result = cy_http_server_network_init();

    /* Allocate memory needed for secure HTTP server. */
    result = cy_http_server_create(&nw_interface, HTTP_PORT, MAX_SOCKETS, NULL, &http_sta_server);
    PRINT_AND_ASSERT(result, "Failed to allocate memory for the HTTP server.\n");

    /* Configure server sent events*/
    dynamic_sse_resource.resource_handler = process_sse_handler;
    dynamic_sse_resource.arg = NULL;
    result = cy_http_server_register_resource( http_sta_server,
                                                (uint8_t*) "/events",
                                                (uint8_t*)"text/event-stream",
                                                CY_RAW_DYNAMIC_URL_CONTENT,
                                                &dynamic_sse_resource);
    PRINT_AND_ASSERT(result, "Failed to register a resource.\n");
    
    /* Configure dynamic resource handler. */
    http_get_post_resource.resource_handler = softap_resource_handler;
    http_get_post_resource.arg = NULL;

    /* Register all the resources with the secure HTTP server. */
    result = cy_http_server_register_resource(http_sta_server,
                                              (uint8_t *)"/",
                                              (uint8_t *)"text/html",
                                              CY_DYNAMIC_URL_CONTENT,
                                              &http_get_post_resource);
    PRINT_AND_ASSERT(result, "Failed to register a resource.\n");

    /* Start the HTTP server. */
    result = cy_http_server_start(http_sta_server);
    PRINT_AND_ASSERT(result, "Failed to start the HTTP server.\n");

    /* Advertise STA web UI hostname on LAN via mDNS. */
    cy_rslt_t mdns_result = start_sta_mdns_service();
    if (mdns_result != CY_RSLT_SUCCESS)
    {
        ERR_INFO(("mDNS start failed during AP->STA reconfigure.\r\n"));
    }
   
    return result;
}

/*******************************************************************************
* Function Name: url_decode
********************************************************************************
*
* Summary:
* This function is used to decode the url encoded HTTP data received.
*
* Parameters:
* char* dst: Pointer to to whch decoded data should be copied.
* const uint8_t* src: Pointer to url encoded data .
*
* Return:
* void: Returns void .
*
*******************************************************************************/
void url_decode(char *dst, const uint8_t *src)
{
    char current_char, next_char;
    /* Ensure that the received character is a valid character. */
    while ((*src) && ((*src) < VALID_CHARACTER_ASCII_VALUE))
    {
        /* URL encoding replaces unsafe ASCII characters with a "%" followed 
         * by two hexadecimal digits. Check for character "%" followed by
         * two hexadecimal digits to decode unsafe ASCII characters.
         */
        if ((*src == MODUS_OPERATOR_ASCII_VALUE) && ((current_char = src[1]) && (next_char = src[2])) && (isxdigit(current_char) && isxdigit(next_char)))
        {
            if (current_char >= SMALL_LETTER_A_ASCII_VALUE)
            {
                current_char -= SMALL_LETTER_A_ASCII_VALUE - CAPITAL_LETTER_A_ASCII_VALUE;
            }
            if (current_char >= CAPITAL_LETTER_A_ASCII_VALUE)
            {
                current_char -= (CAPITAL_LETTER_A_ASCII_VALUE - LF_OPERATOR_ASCII_VALUE);
            }
            else
            {
                current_char -= NUMBER_ZERO_ASCII_VALUE;
            }
            if (next_char >= SMALL_LETTER_A_ASCII_VALUE)
            {
                next_char -= SMALL_LETTER_A_ASCII_VALUE - CAPITAL_LETTER_A_ASCII_VALUE;
            }
            if (next_char >= CAPITAL_LETTER_A_ASCII_VALUE)
            {
                next_char -= (CAPITAL_LETTER_A_ASCII_VALUE - LF_OPERATOR_ASCII_VALUE);
            }
            else
            {
                next_char -= NUMBER_ZERO_ASCII_VALUE ;
            }
            *dst++ = DLE_OPERATOR_ASCII_VALUE * current_char + next_char;
            src += URL_DECODE_ASCII_OFFSET_VALUE;
        }
        /* A space character is URL encoded as "+". Decode space character. */
        else if (*src == PLUS_OPERATOR_ASCII_VALUE)
        {
            *dst++ = SPACE_CHARACTER_ASCII_VALUE;
            src++;
        }
        /* Decode other characters. */
        else
        {
            *dst++ = *src++;
        }

    }

    *dst++ = NULL_CHARACTER_ASCII_VALUE;
    
}

/*******************************************************************************
* Function Name: provisioning_button_init
********************************************************************************
* Summary:
*  Initializes the user button used to force provisioning mode at runtime.
*
* Parameters:
*  void
*
* Return:
*  void
*******************************************************************************/
static void provisioning_button_init(void)
{
    cy_rslt_t result;
    uint8_t inactive_state = (PROVISION_BUTTON_ACTIVE_STATE == 0u) ? 1u : 0u;

    /* User button must be input + pull-up for active-low long-press detection. */
    result = cyhal_gpio_init(PROVISION_FORCE_BUTTON, CYHAL_GPIO_DIR_INPUT,
                             CYHAL_GPIO_DRIVE_PULLUP, inactive_state);
    if (result != CY_RSLT_SUCCESS)
    {
        ERR_INFO(("Provisioning button init failed (0x%08lx). Retrying after free.\r\n",
                  (unsigned long)result));
        cyhal_gpio_free(PROVISION_FORCE_BUTTON);
        result = cyhal_gpio_init(PROVISION_FORCE_BUTTON, CYHAL_GPIO_DIR_INPUT,
                                 CYHAL_GPIO_DRIVE_PULLUP, inactive_state);
    }

    if (result == CY_RSLT_SUCCESS)
    {
        provisioning_button_press_event =
            (PROVISION_BUTTON_ACTIVE_STATE == 0u) ? CYHAL_GPIO_IRQ_FALL : CYHAL_GPIO_IRQ_RISE;
        provisioning_button_release_event =
            (PROVISION_BUTTON_ACTIVE_STATE == 0u) ? CYHAL_GPIO_IRQ_RISE : CYHAL_GPIO_IRQ_FALL;

        cyhal_gpio_register_callback(PROVISION_FORCE_BUTTON, &provisioning_button_cb_data);
        cyhal_gpio_enable_event(PROVISION_FORCE_BUTTON,
                                provisioning_button_press_event | provisioning_button_release_event,
                                PROVISION_BUTTON_INTR_PRIORITY,
                                true);

        provisioning_button_enabled = true;
        provisioning_button_irq_enabled = true;
        APP_INFO(("Provisioning button configured: input/pull-up, active state=%lu.\r\n",
                  (unsigned long)PROVISION_BUTTON_ACTIVE_STATE));
        APP_INFO(("Provisioning button interrupt enabled.\r\n"));
    }
    else
    {
        provisioning_button_enabled = false;
        provisioning_button_irq_enabled = false;
        ERR_INFO(("Failed to initialize provisioning button GPIO (0x%08lx). "
                  "Runtime force-provisioning via button is disabled.\r\n",
                  (unsigned long)result));
    }
}

/*******************************************************************************
* Function Name: start_provisioning_softap
********************************************************************************
* Summary:
*  Starts SoftAP and HTTP provisioning server (AP mode).
*  This function is used on boot fallback (no known network) and also for
*  runtime switching back into provisioning.
*
* Parameters:
*  void
*
* Return:
*  cy_rslt_t: CY_RSLT_SUCCESS on success.
*******************************************************************************/
static cy_rslt_t start_provisioning_softap(void)
{
    cy_rslt_t result;

    APP_INFO(("Entering provisioning/AP mode.\r\n"));
    display_status_show_boot("Provisioning requested", "Starting SoftAP server");

    /* Provisioning mode should not advertise STA mDNS hostname. */
    stop_sta_mdns_service();

    /* Reset provisioning state variables */
    device_configured = false;
    reconfiguration_request = 0;
    http_event_stream = NULL;

    /* Start SoftAP */
    result = start_ap_mode();
    PRINT_AND_ASSERT(result, "start_ap_mode failed.\n");

    /* Create and start HTTP server bound to AP interface */
    result = configure_http_server();
    PRINT_AND_ASSERT(result, "configure_http_server failed.\n");

    result = cy_http_server_start(http_ap_server);
    PRINT_AND_ASSERT(result, "cy_http_server_start(AP) failed.\n");

    /* Print instructions to UART/TFT */
    display_configuration();

    return CY_RSLT_SUCCESS;
}

/*******************************************************************************
* Function Name: stop_sta_http_server
********************************************************************************
* Summary:
*  Stops and deletes the STA HTTP server instance (if it is running).
*  Also deinitializes the HTTP server network stack.
*
* Parameters:
*  void
*
* Return:
*  void
*******************************************************************************/
static void stop_sta_http_server(void)
{
    stop_sta_mdns_service();

    /* Stop STA server if we are in STA configured mode */
    (void)cy_http_server_stop(http_sta_server);
    (void)cy_http_server_delete(http_sta_server);
    (void)cy_http_server_network_deinit();
}

/*******************************************************************************
* Function Name: switch_to_provisioning_mode
********************************************************************************
* Summary:
*  Runtime switch:
*  - Stop STA HTTP server
*  - Disconnect from AP
*  - Start SoftAP provisioning server
*
* Parameters:
*  void
*
* Return:
*  void
*******************************************************************************/
static void switch_to_provisioning_mode(void)
{
    if (reconfiguration_request == 0)
    {
        if (is_provisioning_ap_active())
        {
            return;
        }
    }

    /* Stop STA HTTP server (if running) */
    if (reconfiguration_request == SERVER_RECONFIGURED)
    {
        stop_sta_http_server();
    }

    /* Disconnect from currently connected AP (if any) */
    if (cy_wcm_is_connected_to_ap())
    {
        (void)cy_wcm_disconnect_ap();
    }

    /* Ensure AP is stopped before starting it again */
    (void)cy_wcm_stop_ap();
    /* Start SoftAP provisioning */
    (void)start_provisioning_softap();
}

/*******************************************************************************
* Function Name: handle_runtime_force_provisioning
********************************************************************************
* Summary:
*  Checks long-press on user button. If held for PROVISION_LONG_PRESS_MS,
*  switch device into provisioning mode.
*
* Parameters:
*  uint32_t loop_period_ms: calling loop delay in ms
*
* Return:
*  void
*******************************************************************************/
static void handle_runtime_force_provisioning(uint32_t loop_period_ms)
{
    bool irq_pressed;

    static bool stable_pressed = false;
    static bool last_raw_pressed = false;
    static bool press_handled = false;
    static uint32_t debounce_ms = 0;
    (void)loop_period_ms;

    if (!provisioning_button_enabled)
    {
        return;
    }

    irq_pressed = provisioning_button_irq_pressed;
    (void)provisioning_button_irq_is_pressed;

    provisioning_button_irq_pressed = false;
    provisioning_button_irq_released = false;

    if (provisioning_button_irq_enabled)
    {
        if (irq_pressed)
        {
            APP_INFO(("Button press detected.\r\n"));
            if (is_provisioning_ap_active())
            {
                APP_INFO(("Already in provisioning/AP mode.\r\n"));
            }
            else
            {
                provisioning_button_force_mode_requested = true;
                switch_to_provisioning_mode();
            }
        }

        return;
    }

    bool raw_pressed = (cyhal_gpio_read(PROVISION_FORCE_BUTTON) == PROVISION_BUTTON_ACTIVE_STATE);

    if (raw_pressed != last_raw_pressed)
    {
        last_raw_pressed = raw_pressed;
        debounce_ms = 0;
    }
    else if (debounce_ms < PROVISION_BUTTON_DEBOUNCE_MS)
    {
        debounce_ms += loop_period_ms;
    }

    if ((debounce_ms >= PROVISION_BUTTON_DEBOUNCE_MS) && (stable_pressed != raw_pressed))
    {
        stable_pressed = raw_pressed;

        if (stable_pressed)
        {
            APP_INFO(("Button press detected.\r\n"));
            if (!press_handled)
            {
                press_handled = true;
                if (is_provisioning_ap_active())
                {
                    APP_INFO(("Already in provisioning/AP mode.\r\n"));
                }
                else
                {
                    provisioning_button_force_mode_requested = true;
                    switch_to_provisioning_mode();
                }
            }
        }
        else
        {
            APP_INFO(("Button released.\r\n"));
            press_handled = false;
        }
    }
}

/*******************************************************************************
* Function Name: server_task
********************************************************************************
* Summary:
*  Task that initializes the device networking, starts the HTTP server,
*  and periodically services the LIN actuator state machine.
*
* Parameters:
*  arg - Unused.
*
* Return:
*  None.
*
*******************************************************************************/
void server_task(void *arg)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    (void)arg;

    char http_response[DEVICE_DATA_RESPONSE_LENGTH] = {0};
    char status_buffer[ACTUATOR_STATUS_BUFFER_LENGTH] = {0};

#ifdef ENABLE_OLED
    display_status_init();
    display_status_show_boot("Booting device", "Initializing services");
#endif /* ENABLE_OLED */

    /**************************************************************************
     * Initialize LIN actuator module once before entering the main loop.
     **************************************************************************/
    result = lin_actuator_init();
    PRINT_AND_ASSERT(result, "LIN actuator init failed.\r\n");

    /* Initialize the Wi-Fi device as AP + STA. */
    cy_wcm_config_t config = { .interface = CY_WCM_INTERFACE_TYPE_AP_STA };

    /* Initialize the Wi-Fi device, transport, and lwIP network stack. */
    result = cy_wcm_init(&config);
    PRINT_AND_ASSERT(result, "cy_wcm_init failed...!\n");
    display_status_show_boot("Wi-Fi stack ready", "Checking saved networks");

    /**************************************************************************
     * Runtime provisioning button + network store init
     **************************************************************************/
    provisioning_button_init();
    (void)wifi_store_init();

    /**************************************************************************
     * Try known networks first (STA), fallback to provisioning (AP)
     **************************************************************************/
    wifi_network_t known[WIFI_STORE_MAX_NETWORKS];
    uint32_t known_count = 0;

    result = wifi_store_get_known_networks(known, WIFI_STORE_MAX_NETWORKS, &known_count);
    if (result != CY_RSLT_SUCCESS)
    {
        ERR_INFO(("wifi_store_get_known_networks failed (0x%08lx). Starting provisioning.\r\n",
                  (unsigned long)result));
        known_count = 0;
    }

    bool connected = false;

    for (uint32_t i = 0; i < known_count; i++)
    {
        memset(wifi_ssid, 0, sizeof(wifi_ssid));
        memset(wifi_pwd,  0, sizeof(wifi_pwd));

        strncpy((char*)wifi_ssid, known[i].ssid, WIFI_SSID_LEN - 1);
        strncpy((char*)wifi_pwd,  known[i].pwd,  WIFI_PWD_LEN  - 1);

        APP_INFO(("Boot: trying known network %lu/%lu: %s\r\n",
                  (unsigned long)(i + 1), (unsigned long)known_count, known[i].ssid));
        display_status_show_known_network_attempt(known[i].ssid, i + 1u, known_count);

        result = start_sta_mode();
        if (result == CY_RSLT_SUCCESS)
        {
            connected = true;

            (void)wifi_store_mark_used((const char*)wifi_ssid);

            device_configured = true;
            reconfiguration_request = SERVER_RECONFIGURED;

            result = configure_http_server_sta_only();
            PRINT_AND_ASSERT(result, "Failed to start STA-only HTTP server.\n");

            display_configuration();
            break;
        }
    }

    if (!connected)
    {
        if (is_provisioning_ap_active())
        {
            provisioning_button_force_mode_requested = false;
        }
        else
        {
            ERR_INFO(("Wi-Fi connection failure (%s) and fallback to provisioning/AP mode.\r\n",
                      wifi_fail_reason_to_text(last_wifi_fail_reason)));
            APP_INFO(("Boot: no known network connected. Starting provisioning SoftAP...\r\n"));

            result = start_provisioning_softap();
            PRINT_AND_ASSERT(result, "start_provisioning_softap failed.\n");
        }
    }

    /**************************************************************************
     * Main server loop
     **************************************************************************/
    while (true)
    {
        /* Runtime long-press check in all modes (AP provisioning + STA). */
        handle_runtime_force_provisioning(SERVER_LOOP_PERIOD_MS);
        service_sta_mdns_retry(SERVER_LOOP_PERIOD_MS);

        if (SERVER_RECONFIGURED == reconfiguration_request)
        {
            /* Run one step of the LIN actuator state machine. */
            lin_actuator_task();

            /* Build status text for browser SSE updates. */
            lin_actuator_get_http_status(status_buffer, sizeof(status_buffer));
            snprintf(http_response, sizeof(http_response), "%s", status_buffer);

            display_status_show_sta_status(status_buffer);

            /* Send event stream update to browser */
            if (http_event_stream != NULL)
            {
                result = cy_http_server_response_stream_write_payload(http_event_stream,
                                                                      (const void*)EVENT_STREAM_DATA,
                                                                      sizeof(EVENT_STREAM_DATA) - 1);
                if (result != CY_RSLT_SUCCESS)
                {
                    ERR_INFO(("Updating event stream failed\r\n"));
                    http_event_stream = NULL;
                }

                if (http_event_stream != NULL)
                {
                    result = cy_http_server_response_stream_write_payload(http_event_stream,
                                                                          http_response,
                                                                          strlen(http_response));
                    if (result != CY_RSLT_SUCCESS)
                    {
                        ERR_INFO(("Updating event stream failed\r\n"));
                        http_event_stream = NULL;
                    }
                }

                if (http_event_stream != NULL)
                {
                    result = cy_http_server_response_stream_write_payload(http_event_stream,
                                                                          (const void*)LFLF,
                                                                          sizeof(LFLF) - 1);
                    if (result != CY_RSLT_SUCCESS)
                    {
                        ERR_INFO(("Updating event stream failed\r\n"));
                        http_event_stream = NULL;
                    }
                }
            }
        }

        if (SERVER_RECONFIGURE_REQUESTED == reconfiguration_request)
        {
            reconfigure_http_server();
            display_configuration();
            reconfiguration_request = SERVER_RECONFIGURED;
        }

        vTaskDelay(pdMS_TO_TICKS(SERVER_LOOP_PERIOD_MS));
    }
}

/*******************************************************************************
* Function Name: display_configuration
********************************************************************************
* Summary:
*  Displays details for configuring device on serial terminal and TFT screen.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void display_configuration(void)
{
    cy_wcm_ip_address_t ip_address;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    char display_ip_buffer[DISPLAY_BUFFER_LENGTH];
    char http_url[URL_LENGTH]={0};
    const char *hostname = STA_MDNS_HOSTNAME ".local";

    if(reconfiguration_request == 0)
    {
        /* IP address of SoftAp. */
        result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_AP, &ip_address);
        PRINT_AND_ASSERT(result, "Failed to retrieveSoftAP IP address\n");

        /*Print message to connect to that ip address*/
        format_ipv4_address(ip_address.ip.v4, display_ip_buffer, sizeof(display_ip_buffer));

        sprintf(http_url, "http://%s:%d", display_ip_buffer, HTTP_PORT);

        APP_INFO(("****************************************************************************\r\n"));
        APP_INFO(("Using another device, connect to the following Wi-Fi network:\r\n"));
        APP_INFO(("SSID     : %s\r\n", SOFTAP_SSID));
        APP_INFO(("Password : %s\r\n", SOFTAP_PASSWORD));
        APP_INFO(("Open a web browser of your choice and enter the URL %s\r\n", http_url));
        APP_INFO(("This opens up the the home page for web server application.\r\n"));
        APP_INFO(("You can either enter Wi-Fi network name and password directly or \r\n"));
        APP_INFO(("perform a Wi-Fi scan to get the list of available APs.\r\n"));
        APP_INFO(("****************************************************************************\r\n"));

        display_status_show_provisioning(SOFTAP_SSID, SOFTAP_PASSWORD, display_ip_buffer, NULL);
    }
    else
    {
        /*Variable to store associated AP informations. */
        cy_wcm_associated_ap_info_t associated_ap_info;
        /* IP address of STA. */
        result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &ip_address);
        PRINT_AND_ASSERT(result, "Failed to retrieveSoftAP IP address\n");

        /*Print message to connect to that ip address*/
        format_ipv4_address(ip_address.ip.v4, display_ip_buffer, sizeof(display_ip_buffer));
                
        sprintf(http_url, "http://%s:%d", display_ip_buffer, HTTP_PORT);
        
        /*Get the associated AP informations. */
        cy_wcm_get_associated_ap_info(&associated_ap_info);

        /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen. */
        APP_INFO(("\x1b[2J\x1b[;H"));
        APP_INFO(("******************************************************************\r\n"));
        APP_INFO(("On a device connected to the '%s' Wi-Fi network, \r\n", associated_ap_info.SSID));
        APP_INFO(("Open a web browser and go to : %s\r\n", http_url));
#if LWIP_MDNS_RESPONDER
        APP_INFO(("Or use mDNS hostname URL      : http://%s.local/\r\n", STA_MDNS_HOSTNAME));
#endif
#ifdef ENABLE_OLED
        APP_INFO(("Use the webpage to monitor LIN actuator status updates.\r\n"));
#endif /* ENABLE_OLED */
        APP_INFO(("Use the webpage controls to CALIBRATE, OPEN, or CLOSE the LIN actuator.\r\n"));
        APP_INFO(("******************************************************************\r\n"));

        display_status_show_sta_ready((const char *)associated_ap_info.SSID, display_ip_buffer, hostname);
    }
}

/* [] END OF FILE */
