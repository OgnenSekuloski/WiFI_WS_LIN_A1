/******************************************************************************
* File Name:   main.c
*
* Description: This application demonstrates a PSoC 6 device hosting an http
*              webserver. The PSoC 6 measures the voltage of the ambient light
*              sensor on the CY8CKIT-028. It then displays that information on
*              a webpage. The PSoC 6 also controls the brightness of the RED
*              led on the board. The brightness can be controlled by the two
*              CAPSENSE buttons, CAPSENSE slider, or the webpage.
*
* Related Document: README.md
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
#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* FreeRTOS headers */
#include <FreeRTOS.h>
#include <task.h>

/* Server task header file */
#include "web_server.h"

#if defined(CY_DEVICE_PSOC6A512K)
#include "cy_serial_flash_qspi.h"
#include "cycfg_qspi_memslot.h"
#endif


/*******************************************************************************
* Macros
******************************************************************************/
/* RTOS related macros. */
#define SERVER_TASK_STACK_SIZE        (10 * 1024)
#define SERVER_TASK_PRIORITY          (1)

/*******************************************************************************
* Global Variables
********************************************************************************/
/* This enables RTOS aware debugging */
volatile int uxTopUsedPriority;

/* SOFTAP server task handle. */
TaskHandle_t server_task_handle;

/*******************************************************************************
 * Function Name: main
 *******************************************************************************
 * Summary:
 *  Entry function for the application.
 *  This function initializes the BSP, UART port for debugging, initializes the
 *  user LED on the kit, and starts the RTOS scheduler.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int: Should never return.
 *
 *******************************************************************************/
int main(void)
{
    /* Variable to capture return value of functions */
    cy_rslt_t result;

    /* This enables RTOS aware debugging in OpenOCD */
    uxTopUsedPriority = configMAX_PRIORITIES - 1;

    /* Initialize the Board Support Package (BSP) */
    result = cybsp_init();
    CHECK_RESULT(result);

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);


    #if defined(CY_DEVICE_PSOC6A512K)
        const uint32_t bus_frequency = 50000000lu;
        cy_serial_flash_qspi_init(smifMemConfigs[0], CYBSP_QSPI_D0, CYBSP_QSPI_D1,
                                      CYBSP_QSPI_D2, CYBSP_QSPI_D3, NC, NC, NC, NC,
                                      CYBSP_QSPI_SCK, CYBSP_QSPI_SS, bus_frequency);

        cy_serial_flash_qspi_enable_xip(true);
    #endif

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen */
    APP_INFO(("\x1b[2J\x1b[;H"));
    printf("Visit the below link for step by step instructions to run this code example:\n\n");
    printf("https://github.com/Infineon/mtb-example-anycloud-wifi-web-server#operation\n\n");
    APP_INFO(("============================================================\n"));
    APP_INFO(("               Wi-Fi Web Server                   \n"));
    APP_INFO(("============================================================\n\n"));

    /* Starts the SoftAP and then HTTP server . */
    xTaskCreate(server_task, "HTTP Web Server", SERVER_TASK_STACK_SIZE, NULL,
                SERVER_TASK_PRIORITY, &server_task_handle);

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler();

    /* Should never get here */
    CY_ASSERT(0);
}

/* [] END OF FILE */
