/*******************************************************************************
* File Name:   display_status.h
*
* Description: This file declares the display status helper APIs used to show
*              Wi-Fi and LIN actuator state transitions on the attached
*              display module.
*
*******************************************************************************/

#ifndef DISPLAY_STATUS_H_
#define DISPLAY_STATUS_H_

#include <stdint.h>

void display_status_init(void);
void display_status_show_boot(const char *line1, const char *line2);
void display_status_show_known_network_attempt(const char *ssid, uint32_t index, uint32_t total);
void display_status_show_scanning(void);
void display_status_show_connecting(const char *ssid, uint32_t attempt, uint32_t total);
void display_status_show_connect_success(const char *ssid, const char *ip, const char *hostname);
void display_status_show_connect_failure(const char *ssid, const char *reason);
void display_status_show_provisioning(const char *softap_ssid, const char *softap_password,
                                      const char *ap_ip, const char *hostname);
void display_status_show_sta_ready(const char *sta_ssid, const char *sta_ip, const char *hostname);
void display_status_show_sta_status(const char *status_text);

#endif /* DISPLAY_STATUS_H_ */
