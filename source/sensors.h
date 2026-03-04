/******************************************************************************
* File Name: sensors.h
*
* Description: This file contains configuration parameters for configuring the
*              sensors.
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

/*******************************************************************************
* Include guard
*******************************************************************************/
#ifndef SENSORS_H_
#define SENSORS_H_


/* PWM LED frequency in Hz */
#define PWM_LED_FREQ_HZ                                 (1000000lu)

/* Returns duty cycle */
#define GET_DUTY_CYCLE(x)                               (100 - x)

 /* Default LED dutycycle */
#define DEFAULT_DUTYCYCLE                               (50u)

 /* Maximum dutycycle */
#define MAX_DUTYCYCLE                                   (100u)

/* Minimum dutycycle */
#define MIN_DUTYCYCLE                                   (2u)

/* Dutycycle increment/decrement by 10% */
#define DUTYCYCLE_INCREMENT                             (10u)

/* CAPSENSE task stack size */
#define CAPSENSE_TASK_STACK_SIZE                        (5 * 1024)

/* CAPSENSE task priority */
#define CAPSENSE_TASK_PRIORITY                          (1u)

/* CAPSENSE interrupt priority */
#define CAPSENSE_INTR_PRIORITY                          (7u)

#ifdef ENABLE_TFT
/* Light sensor pin mapped to red led */
#define LIGHT_SENSOR_PIN                                (CYBSP_A0)

/* Max voltage of ADC reading  */
#define LIGHTSENSOR_ADC_MAX_VOLTAGE                     (3300u)

/* Max count of ADC reading  */
#define LIGHTSENSOR_ADC_MAX_COUNT                       (100u)

/* Initial row position on TFT display */
#define TOP_DISPLAY                                     (0u)

#endif /* #ifdef ENABLE_TFT */

/* Timeout value to get mutex */
#define GET_MUTEX_DELAY                                 (200u)
/*******************************************************************************
 *                    Structures
*******************************************************************************/
typedef struct
{
    uint8_t         duty;
    SemaphoreHandle_t xpwm_mutex;
} pwm_duty_t;

/*******************************************************************************
 * Function Prototypes
*******************************************************************************/
uint32_t initialize_led(void);
uint32_t initialize_capsense(void);
void capsense_callback(cy_stc_active_scan_sns_t * ptrActiveScan);
void adjust_led_brightness(void);
void increase_duty_cycle(void);
void decrease_duty_cycle(void);
void set_duty_cycle(uint32_t duty_cycle);
uint8_t get_duty_cycle(void);
void process_touch(void);
void initialize_sensors(void);

#ifdef ENABLE_TFT
    uint32_t initialize_light_sensor(void);
#endif /* #ifdef ENABLE_TFT */

#endif
