/******************************************************************************
* File Name: lin_actuator.h
*
* Description:
*   This file contains the public API for controlling the LIN actuator from
*   the web server application.
*
*   The LIN actuator module is responsible for:
*     - Initializing the LIN UART/SCB blocks
*     - Formatting LIN command frames
*     - Requesting actuator status frames
*     - Parsing actuator response bytes
*     - Maintaining the actuator runtime state machine
*     - Providing human-readable status text for UART logging and SSE updates
*
*******************************************************************************/

#ifndef LIN_ACTUATOR_H_
#define LIN_ACTUATOR_H_

#include "cyhal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*******************************************************************************
 * Enumerations
 *******************************************************************************/
typedef enum
{
    LIN_ACTUATOR_STATE_UNINITIALIZED = 0,
    LIN_ACTUATOR_STATE_IDLE,
    LIN_ACTUATOR_STATE_CALIBRATING_TO_CLOSED,
    LIN_ACTUATOR_STATE_CALIBRATING_TO_OPEN,
    LIN_ACTUATOR_STATE_READY,
    LIN_ACTUATOR_STATE_OPENING,
    LIN_ACTUATOR_STATE_CLOSING,
    LIN_ACTUATOR_STATE_ERROR
} lin_actuator_state_t;

/*******************************************************************************
 * Function Prototypes
 *******************************************************************************/
cy_rslt_t lin_actuator_init(void);
void lin_actuator_task(void);

void lin_actuator_request_calibration(void);
void lin_actuator_request_open(void);
void lin_actuator_request_close(void);

lin_actuator_state_t lin_actuator_get_state(void);
bool lin_actuator_is_ready(void);
const char* lin_actuator_get_state_string(void);
const char* lin_actuator_get_last_debug_string(void);

void lin_actuator_get_http_status(char *buffer, size_t buffer_len);

#endif /* LIN_ACTUATOR_H_ */