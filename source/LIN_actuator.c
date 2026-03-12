/******************************************************************************
* File Name: lin_actuator.c
*
* Description:
*   This file contains the LIN actuator implementation used by the web server
*   application.
*
*   Design goals:
*     - Keep all LIN-specific logic outside web_server.c
*     - Replace blocking calibration loops with a simple runtime state machine
*     - Provide rich UART debug output for bring-up without the real actuator
*     - Provide a text status string suitable for Server-Sent Events (SSE)
*
*******************************************************************************/

#include "LIN_actuator.h"

#include "cyhal_system.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*******************************************************************************
 * Local Macros
 *******************************************************************************/

/* LIN timing / protocol configuration */
#define LIN_BREAK_BITS                            (13u)
#define LIN_FRAME_DATA_LEN                        (8u)
#define LIN_STATUS_RESPONSE_LEN                   (12u)

/* Actuator-specific LIN IDs.
 * These values come from the colleague's working demo:
 *   0x32 -> command frame ID
 *   0x33 -> status request header ID
 */
#define LIN_COMMAND_ID                            (0x32u)
#define LIN_STATUS_REQUEST_ID                     (0x33u)

/* Command timing.
 * The task is expected to be called periodically from the server loop.
 * A command/status exchange is performed every LIN_EXCHANGE_PERIOD_MS
 * while the actuator is in an active state.
 */
#define LIN_EXCHANGE_PERIOD_MS                    (100u)

/* Actuator status byte interpretation.
 * In the received response:
 *   - rx_data[2] must match 0x73 to identify the expected status payload
 *   - bits [2:1] of rx_data[4] indicate calibration progress
 */
#define LIN_STATUS_SIGNATURE_INDEX                (2u)
#define LIN_STATUS_SIGNATURE_VALUE                (0x73u)
#define LIN_STATUS_FLAGS_INDEX                    (4u)
#define LIN_STATUS_FLAGS_MASK                     (0x06u)
#define LIN_STATUS_CLOSED_LEARNED_VALUE           (0x02u)
#define LIN_STATUS_ALL_LEARNED_VALUE              (0x00u)

/* Command byte formatting */
#define LIN_CMD_CALIBRATION_MASK                  (0x03u)
#define LIN_CMD_FLAP_OPEN_MASK                    (0xF0u)

/* Command encoding */
#define LIN_CALIBRATION_IDLE                      (0u)
#define LIN_CALIBRATION_START                     (1u)

#define LIN_FLAP_CLOSE                            (0u)
#define LIN_FLAP_OPEN                             (1u)

/* UART debug / status buffer sizes */
#define LIN_DEBUG_TEXT_LEN                        (128u)
#define LIN_HTTP_STATUS_LEN                       (256u)

/* If real actuator is not available yet, keep this enabled.
 * Simulation mode fabricates expected status transitions so the entire
 * web-to-LIN control path can be tested from browser + UART terminal.
 *
 * Set to 0 when testing with the real actuator.
 */
#define LIN_ACTUATOR_ENABLE_SIMULATION            (0u)

/* Task period used by the existing web server loop */
#define LIN_TASK_CALL_PERIOD_MS                   (50u)

#if !LIN_ACTUATOR_ENABLE_SIMULATION
#include "cy_pdl.h"
#include "cy_scb_uart.h"
#include "cycfg_peripherals.h"

/* LIN SCB blocks configured in Device Configurator. */
#define LIN_TX_SCB_INSTANCE                       (SCB5)
#define LIN_RX_SCB_INSTANCE                       (SCB10)
#define LIN_TX_SCB_CONFIG                         (scb_5_config)
#define LIN_RX_SCB_CONFIG                         (scb_10_config)
#endif

/*******************************************************************************
 * Local Types
 *******************************************************************************/
typedef enum
{
    LIN_REQUEST_NONE = 0,
    LIN_REQUEST_CALIBRATE,
    LIN_REQUEST_OPEN,
    LIN_REQUEST_CLOSE
} lin_request_t;

typedef struct
{
    lin_actuator_state_t state;
    lin_request_t pending_request;

    bool initialized;
    bool all_positions_learned;

    uint8_t tx_data[LIN_FRAME_DATA_LEN];
    uint8_t rx_data[LIN_STATUS_RESPONSE_LEN];

    uint32_t elapsed_since_exchange_ms;
    uint32_t simulated_exchange_counter;

    char last_debug[LIN_DEBUG_TEXT_LEN];
} lin_actuator_context_t;

/*******************************************************************************
 * Local Variables
 *******************************************************************************/
#if !LIN_ACTUATOR_ENABLE_SIMULATION
static cy_stc_scb_uart_context_t lin_tx_uart_context;
static cy_stc_scb_uart_context_t lin_rx_uart_context;
#endif
static lin_actuator_context_t lin_ctx;

/*******************************************************************************
 * Local Function Prototypes
 *******************************************************************************/
static void lin_set_debug(const char *text);
static void lin_set_debug_fmt(const char *fmt, ...);

static void lin_prepare_command(uint8_t calibration_cmd, uint8_t flap_pos);
static void lin_send_command_frame(uint8_t raw_id, const uint8_t *data, size_t len);

static bool lin_request_and_read_status(void);
static bool lin_parse_expected_status(bool *closed_position_learned, bool *all_positions_learned);

static void lin_handle_pending_request(void);
static void lin_execute_state_machine_step(void);

static const char* lin_state_to_string(lin_actuator_state_t state);

#if !LIN_ACTUATOR_ENABLE_SIMULATION
static uint8_t lin_protected_id(uint8_t id);
static uint8_t lin_checksum_enhanced(uint8_t id, const uint8_t *data, size_t len);
static void lin_send_header_request(uint8_t raw_id);
#endif

/*******************************************************************************
 * Function Name: lin_set_debug
 *******************************************************************************
 * Summary:
 *  Updates the cached human-readable debug string.
 *
 * Parameters:
 *  text - New debug message.
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void lin_set_debug(const char *text)
{
    memset(lin_ctx.last_debug, 0, sizeof(lin_ctx.last_debug));
    snprintf(lin_ctx.last_debug, sizeof(lin_ctx.last_debug), "%s", text);
}

/*******************************************************************************
 * Function Name: lin_set_debug_fmt
 *******************************************************************************
 * Summary:
 *  Formats and stores a debug string, then prints it to UART.
 *
 * Parameters:
 *  fmt - printf-style format string
 *  ... - format arguments
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void lin_set_debug_fmt(const char *fmt, ...)
{
    va_list args;

    memset(lin_ctx.last_debug, 0, sizeof(lin_ctx.last_debug));

    va_start(args, fmt);
    vsnprintf(lin_ctx.last_debug, sizeof(lin_ctx.last_debug), fmt, args);
    va_end(args);

    printf("Info: LIN: %s\r\n", lin_ctx.last_debug);
}

/*******************************************************************************
 * Function Name: lin_prepare_command
 *******************************************************************************
 * Summary:
 *  Formats byte 0 of the 8-byte LIN payload:
 *    - bits [1:0] contain the calibration command
 *    - bits [7:4] contain the flap position request
 *
 * Parameters:
 *  calibration_cmd - 0 = idle, 1 = start calibration
 *  flap_pos        - 0 = close, 1 = open
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void lin_prepare_command(uint8_t calibration_cmd, uint8_t flap_pos)
{
    memset(lin_ctx.tx_data, 0, sizeof(lin_ctx.tx_data));

    lin_ctx.tx_data[0] = (calibration_cmd & LIN_CMD_CALIBRATION_MASK);

    if (flap_pos == LIN_FLAP_OPEN)
    {
        lin_ctx.tx_data[0] |= LIN_CMD_FLAP_OPEN_MASK;
    }
}

#if !LIN_ACTUATOR_ENABLE_SIMULATION
/*******************************************************************************
 * Function Name: lin_checksum_enhanced
 *******************************************************************************
 * Summary:
 *  Computes LIN enhanced checksum.
 *
 * Parameters:
 *  id   - LIN protected identifier used in frame
 *  data - Payload bytes
 *  len  - Payload length
 *
 * Return:
 *  uint8_t - Enhanced checksum byte
 *
 *******************************************************************************/
static uint8_t lin_checksum_enhanced(uint8_t id, const uint8_t *data, size_t len)
{
    uint16_t sum = 0u;
    size_t i;

    if ((id != 0x3Cu) && (id != 0x3Du))
    {
        sum += id;
    }

    for (i = 0u; i < len; i++)
    {
        sum += data[i];
    }

    while (sum > 0xFFu)
    {
        sum = (sum & 0xFFu) + (sum >> 8);
    }

    return (uint8_t)(~sum);
}

/*******************************************************************************
 * Function Name: lin_protected_id
 *******************************************************************************
 * Summary:
 *  Computes LIN protected ID (PID) from the 6-bit raw frame ID.
 *
 * Parameters:
 *  id - Raw LIN frame ID
 *
 * Return:
 *  uint8_t - Protected ID including parity bits
 *
 *******************************************************************************/
static uint8_t lin_protected_id(uint8_t id)
{
    uint8_t id0 = (id >> 0) & 1u;
    uint8_t id1 = (id >> 1) & 1u;
    uint8_t id2 = (id >> 2) & 1u;
    uint8_t id3 = (id >> 3) & 1u;
    uint8_t id4 = (id >> 4) & 1u;
    uint8_t id5 = (id >> 5) & 1u;

    uint8_t p0 = id0 ^ id1 ^ id2 ^ id4;
    uint8_t p1 = (uint8_t)(!(id1 ^ id3 ^ id4 ^ id5));

    return (uint8_t)((id & 0x3Fu) | (p0 << 6) | (p1 << 7));
}
#endif

/*******************************************************************************
 * Function Name: lin_send_command_frame
 *******************************************************************************
 * Summary:
 *  Sends a full LIN command frame:
 *    BREAK + SYNC + PID + DATA + CHECKSUM
 *
 * Parameters:
 *  raw_id - Raw LIN frame ID
 *  data   - Pointer to payload
 *  len    - Number of payload bytes
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void lin_send_command_frame(uint8_t raw_id, const uint8_t *data, size_t len)
{
#if LIN_ACTUATOR_ENABLE_SIMULATION
    (void)len;
    printf("Info: LIN: SIM command frame. RAW_ID=0x%02X DATA0=0x%02X\r\n",
           raw_id, data[0]);
#else
    uint8_t pid;
    uint8_t checksum;
    size_t i;

    pid = lin_protected_id(raw_id);
    checksum = lin_checksum_enhanced(pid, data, len);

    /* Clear FIFOs before a new frame exchange. */
    Cy_SCB_ClearTxFifo(LIN_TX_SCB_INSTANCE);
    Cy_SCB_ClearRxFifo(LIN_TX_SCB_INSTANCE);

    /* Send BREAK field. */
    Cy_SCB_UART_SendBreakBlocking(LIN_TX_SCB_INSTANCE, LIN_BREAK_BITS);
    cyhal_system_delay_us(55);

    /* Send frame bytes. */
    Cy_SCB_UART_Put(LIN_TX_SCB_INSTANCE, 0x55u);
    Cy_SCB_UART_Put(LIN_TX_SCB_INSTANCE, pid);

    for (i = 0u; i < len; i++)
    {
        Cy_SCB_UART_Put(LIN_TX_SCB_INSTANCE, data[i]);
    }

    Cy_SCB_UART_Put(LIN_TX_SCB_INSTANCE, checksum);

    while (Cy_SCB_UART_GetNumInTxFifo(LIN_TX_SCB_INSTANCE) != 0u)
    {
    }

    printf("Info: LIN: Command frame sent. RAW_ID=0x%02X PID=0x%02X DATA0=0x%02X CHECKSUM=0x%02X\r\n",
           raw_id, pid, data[0], checksum);
#endif
}

#if !LIN_ACTUATOR_ENABLE_SIMULATION
/*******************************************************************************
 * Function Name: lin_send_header_request
 *******************************************************************************
 * Summary:
 *  Sends a LIN header requesting the actuator to publish its response frame:
 *    BREAK + SYNC + PID
 *
 * Parameters:
 *  raw_id - Raw LIN frame ID used to request status
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void lin_send_header_request(uint8_t raw_id)
{
    uint8_t pid = lin_protected_id(raw_id);

    Cy_SCB_ClearTxFifo(LIN_TX_SCB_INSTANCE);
    Cy_SCB_ClearRxFifo(LIN_TX_SCB_INSTANCE);

    Cy_SCB_UART_SendBreakBlocking(LIN_TX_SCB_INSTANCE, LIN_BREAK_BITS);
    cyhal_system_delay_us(55);

    Cy_SCB_UART_Put(LIN_TX_SCB_INSTANCE, 0x55u);
    cyhal_system_delay_us(55);
    Cy_SCB_UART_Put(LIN_TX_SCB_INSTANCE, pid);

    while (Cy_SCB_UART_GetNumInTxFifo(LIN_TX_SCB_INSTANCE) != 0u)
    {
    }

    printf("Info: LIN: Status header sent. RAW_ID=0x%02X PID=0x%02X\r\n", raw_id, pid);
}
#endif

/*******************************************************************************
 * Function Name: lin_request_and_read_status
 *******************************************************************************
 * Summary:
 *  Requests actuator status and reads the response bytes.
 *
 *  In simulation mode, a synthetic response is generated so the complete
 *  software flow can be tested without the real actuator.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  bool - true if a status payload was obtained, false otherwise
 *
 *******************************************************************************/
static bool lin_request_and_read_status(void)
{
#if LIN_ACTUATOR_ENABLE_SIMULATION
    memset(lin_ctx.rx_data, 0, sizeof(lin_ctx.rx_data));

    lin_ctx.simulated_exchange_counter++;

    lin_ctx.rx_data[LIN_STATUS_SIGNATURE_INDEX] = LIN_STATUS_SIGNATURE_VALUE;

    switch (lin_ctx.state)
    {
        case LIN_ACTUATOR_STATE_CALIBRATING_TO_CLOSED:
            if (lin_ctx.simulated_exchange_counter >= 3u)
            {
                lin_ctx.rx_data[LIN_STATUS_FLAGS_INDEX] = LIN_STATUS_CLOSED_LEARNED_VALUE;
            }
            else
            {
                lin_ctx.rx_data[LIN_STATUS_FLAGS_INDEX] = 0x06u;
            }
            break;

        case LIN_ACTUATOR_STATE_CALIBRATING_TO_OPEN:
            if (lin_ctx.simulated_exchange_counter >= 6u)
            {
                lin_ctx.rx_data[LIN_STATUS_FLAGS_INDEX] = LIN_STATUS_ALL_LEARNED_VALUE;
            }
            else
            {
                lin_ctx.rx_data[LIN_STATUS_FLAGS_INDEX] = LIN_STATUS_CLOSED_LEARNED_VALUE;
            }
            break;

        default:
            lin_ctx.rx_data[LIN_STATUS_FLAGS_INDEX] = LIN_STATUS_ALL_LEARNED_VALUE;
            break;
    }

    printf("Info: LIN: SIM status generated. BYTE2=0x%02X BYTE4=0x%02X\r\n",
           lin_ctx.rx_data[LIN_STATUS_SIGNATURE_INDEX],
           lin_ctx.rx_data[LIN_STATUS_FLAGS_INDEX]);

    return true;
#else
    size_t rx_index = 0u;
    uint32_t rx_fifo_count;

    Cy_SCB_ClearRxFifo(LIN_RX_SCB_INSTANCE);

    cyhal_system_delay_ms(40);
    lin_send_header_request(LIN_STATUS_REQUEST_ID);
    cyhal_system_delay_ms(20);

    rx_fifo_count = Cy_SCB_UART_GetNumInRxFifo(LIN_RX_SCB_INSTANCE);
    printf("Info: LIN: RX FIFO count after status request = %lu\r\n", (unsigned long)rx_fifo_count);

    if (rx_fifo_count < LIN_STATUS_RESPONSE_LEN)
    {
        lin_set_debug("No complete actuator response available yet.");
        return false;
    }

    memset(lin_ctx.rx_data, 0, sizeof(lin_ctx.rx_data));

    while (rx_index < LIN_STATUS_RESPONSE_LEN)
    {
        lin_ctx.rx_data[rx_index] = (uint8_t)Cy_SCB_UART_Get(LIN_RX_SCB_INSTANCE);
        rx_index++;
    }

    printf("Info: LIN: Status response received. BYTE2=0x%02X BYTE4=0x%02X\r\n",
           lin_ctx.rx_data[LIN_STATUS_SIGNATURE_INDEX],
           lin_ctx.rx_data[LIN_STATUS_FLAGS_INDEX]);

    return true;
#endif
}

/*******************************************************************************
 * Function Name: lin_parse_expected_status
 *******************************************************************************
 * Summary:
 *  Parses the received actuator response.
 *
 * Parameters:
 *  closed_position_learned - output flag
 *  all_positions_learned   - output flag
 *
 * Return:
 *  bool - true if the received response looks like the expected actuator status
 *
 *******************************************************************************/
static bool lin_parse_expected_status(bool *closed_position_learned, bool *all_positions_learned)
{
    uint8_t flags;

    *closed_position_learned = false;
    *all_positions_learned = false;

    if (lin_ctx.rx_data[LIN_STATUS_SIGNATURE_INDEX] != LIN_STATUS_SIGNATURE_VALUE)
    {
        lin_set_debug_fmt("Unexpected status signature: 0x%02X",
                          lin_ctx.rx_data[LIN_STATUS_SIGNATURE_INDEX]);
        return false;
    }

    flags = (uint8_t)(lin_ctx.rx_data[LIN_STATUS_FLAGS_INDEX] & LIN_STATUS_FLAGS_MASK);

    if (flags == LIN_STATUS_CLOSED_LEARNED_VALUE)
    {
        *closed_position_learned = true;
    }

    if (flags == LIN_STATUS_ALL_LEARNED_VALUE)
    {
        *all_positions_learned = true;
    }

    printf("Info: LIN: Parsed status. FLAGS=0x%02X CLOSED_LEARNED=%u ALL_LEARNED=%u\r\n",
           flags,
           (unsigned int)(*closed_position_learned),
           (unsigned int)(*all_positions_learned));

    return true;
}

/*******************************************************************************
 * Function Name: lin_handle_pending_request
 *******************************************************************************
 * Summary:
 *  Consumes any pending web-requested actuator action.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void lin_handle_pending_request(void)
{
    if (lin_ctx.pending_request == LIN_REQUEST_NONE)
    {
        return;
    }

    switch (lin_ctx.pending_request)
    {
        case LIN_REQUEST_CALIBRATE:
            lin_ctx.state = LIN_ACTUATOR_STATE_CALIBRATING_TO_CLOSED;
            lin_ctx.all_positions_learned = false;
            lin_ctx.simulated_exchange_counter = 0u;
            lin_set_debug("Calibration started. Moving toward closed learned position.");
            break;

        case LIN_REQUEST_OPEN:
            if (!lin_ctx.all_positions_learned)
            {
                lin_set_debug("Open request ignored. Actuator is not calibrated yet.");
            }
            else
            {
                lin_ctx.state = LIN_ACTUATOR_STATE_OPENING;
                lin_set_debug("Open command accepted.");
            }
            break;

        case LIN_REQUEST_CLOSE:
            if (!lin_ctx.all_positions_learned)
            {
                lin_set_debug("Close request ignored. Actuator is not calibrated yet.");
            }
            else
            {
                lin_ctx.state = LIN_ACTUATOR_STATE_CLOSING;
                lin_set_debug("Close command accepted.");
            }
            break;

        default:
            break;
    }

    lin_ctx.pending_request = LIN_REQUEST_NONE;
}

/*******************************************************************************
 * Function Name: lin_execute_state_machine_step
 *******************************************************************************
 * Summary:
 *  Executes one step of the actuator runtime state machine.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
static void lin_execute_state_machine_step(void)
{
    bool got_status;
    bool closed_position_learned;
    bool all_positions_learned;

    switch (lin_ctx.state)
    {
        case LIN_ACTUATOR_STATE_IDLE:
        case LIN_ACTUATOR_STATE_READY:
            /* Nothing to do unless a new request arrives. */
            break;

        case LIN_ACTUATOR_STATE_CALIBRATING_TO_CLOSED:
            lin_prepare_command(LIN_CALIBRATION_START, LIN_FLAP_CLOSE);
            lin_send_command_frame(LIN_COMMAND_ID, lin_ctx.tx_data, LIN_FRAME_DATA_LEN);

            got_status = lin_request_and_read_status();
            if (got_status && lin_parse_expected_status(&closed_position_learned, &all_positions_learned))
            {
                if (closed_position_learned)
                {
                    lin_ctx.state = LIN_ACTUATOR_STATE_CALIBRATING_TO_OPEN;
                    lin_set_debug("Closed position learned. Continuing calibration toward open.");
                }
            }
            break;

        case LIN_ACTUATOR_STATE_CALIBRATING_TO_OPEN:
            lin_prepare_command(LIN_CALIBRATION_START, LIN_FLAP_OPEN);
            lin_send_command_frame(LIN_COMMAND_ID, lin_ctx.tx_data, LIN_FRAME_DATA_LEN);

            got_status = lin_request_and_read_status();
            if (got_status && lin_parse_expected_status(&closed_position_learned, &all_positions_learned))
            {
                if (all_positions_learned)
                {
                    lin_ctx.all_positions_learned = true;
                    lin_ctx.state = LIN_ACTUATOR_STATE_READY;
                    lin_set_debug("Calibration finished. All positions learned. Actuator is ready.");
                }
            }
            break;

        case LIN_ACTUATOR_STATE_OPENING:
            lin_prepare_command(LIN_CALIBRATION_IDLE, LIN_FLAP_OPEN);
            lin_send_command_frame(LIN_COMMAND_ID, lin_ctx.tx_data, LIN_FRAME_DATA_LEN);
            (void)lin_request_and_read_status();

            lin_ctx.state = LIN_ACTUATOR_STATE_READY;
            lin_set_debug("Open command frame sent.");
            break;

        case LIN_ACTUATOR_STATE_CLOSING:
            lin_prepare_command(LIN_CALIBRATION_IDLE, LIN_FLAP_CLOSE);
            lin_send_command_frame(LIN_COMMAND_ID, lin_ctx.tx_data, LIN_FRAME_DATA_LEN);
            (void)lin_request_and_read_status();

            lin_ctx.state = LIN_ACTUATOR_STATE_READY;
            lin_set_debug("Close command frame sent.");
            break;

        case LIN_ACTUATOR_STATE_ERROR:
        case LIN_ACTUATOR_STATE_UNINITIALIZED:
        default:
            break;
    }
}

/*******************************************************************************
 * Function Name: lin_state_to_string
 *******************************************************************************
 * Summary:
 *  Converts actuator state enum to readable text.
 *
 * Parameters:
 *  state - Internal actuator state
 *
 * Return:
 *  const char* - Human-readable state text
 *
 *******************************************************************************/
static const char* lin_state_to_string(lin_actuator_state_t state)
{
    switch (state)
    {
        case LIN_ACTUATOR_STATE_UNINITIALIZED:
            return "UNINITIALIZED";
        case LIN_ACTUATOR_STATE_IDLE:
            return "IDLE";
        case LIN_ACTUATOR_STATE_CALIBRATING_TO_CLOSED:
            return "CALIBRATING_TO_CLOSED";
        case LIN_ACTUATOR_STATE_CALIBRATING_TO_OPEN:
            return "CALIBRATING_TO_OPEN";
        case LIN_ACTUATOR_STATE_READY:
            return "READY";
        case LIN_ACTUATOR_STATE_OPENING:
            return "OPENING";
        case LIN_ACTUATOR_STATE_CLOSING:
            return "CLOSING";
        case LIN_ACTUATOR_STATE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

/*******************************************************************************
 * Function Name: lin_actuator_init
 *******************************************************************************
 * Summary:
 *  Initializes the LIN actuator module and both SCB UART blocks.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t - CY_RSLT_SUCCESS on success
 *
 *******************************************************************************/
cy_rslt_t lin_actuator_init(void)
{
    memset(&lin_ctx, 0, sizeof(lin_ctx));
    lin_ctx.state = LIN_ACTUATOR_STATE_UNINITIALIZED;

#if !LIN_ACTUATOR_ENABLE_SIMULATION
    cy_rslt_t result;

    result = Cy_SCB_UART_Init(LIN_TX_SCB_INSTANCE, &LIN_TX_SCB_CONFIG, &lin_tx_uart_context);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Error: LIN: Failed to initialize TX SCB/UART.\r\n");
        lin_ctx.state = LIN_ACTUATOR_STATE_ERROR;
        return result;
    }
    Cy_SCB_UART_Enable(LIN_TX_SCB_INSTANCE);

    result = Cy_SCB_UART_Init(LIN_RX_SCB_INSTANCE, &LIN_RX_SCB_CONFIG, &lin_rx_uart_context);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Error: LIN: Failed to initialize RX SCB/UART.\r\n");
        lin_ctx.state = LIN_ACTUATOR_STATE_ERROR;
        return result;
    }
    Cy_SCB_UART_Enable(LIN_RX_SCB_INSTANCE);
#endif

    lin_ctx.initialized = true;
    lin_ctx.state = LIN_ACTUATOR_STATE_IDLE;
    lin_set_debug("LIN actuator module initialized.");

#if LIN_ACTUATOR_ENABLE_SIMULATION
    printf("Info: LIN: Simulation mode is ENABLED. No real actuator response is required.\r\n");
#else
    printf("Info: LIN: Simulation mode is DISABLED. Real actuator response is expected.\r\n");
#endif

    return CY_RSLT_SUCCESS;
}

/*******************************************************************************
 * Function Name: lin_actuator_task
 *******************************************************************************
 * Summary:
 *  Periodic LIN actuator task. Call this once every server loop iteration.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void lin_actuator_task(void)
{
    if (!lin_ctx.initialized)
    {
        return;
    }

    lin_handle_pending_request();

    lin_ctx.elapsed_since_exchange_ms += LIN_TASK_CALL_PERIOD_MS;

    if (lin_ctx.elapsed_since_exchange_ms < LIN_EXCHANGE_PERIOD_MS)
    {
        return;
    }

    lin_ctx.elapsed_since_exchange_ms = 0u;

    lin_execute_state_machine_step();
}

/*******************************************************************************
 * Function Name: lin_actuator_request_calibration
 *******************************************************************************
 * Summary:
 *  Queues a calibration request from the web interface.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void lin_actuator_request_calibration(void)
{
    lin_ctx.pending_request = LIN_REQUEST_CALIBRATE;
    printf("Info: LIN: Calibration requested from web UI.\r\n");
}

/*******************************************************************************
 * Function Name: lin_actuator_request_open
 *******************************************************************************
 * Summary:
 *  Queues an open request from the web interface.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void lin_actuator_request_open(void)
{
    lin_ctx.pending_request = LIN_REQUEST_OPEN;
    printf("Info: LIN: Open requested from web UI.\r\n");
}

/*******************************************************************************
 * Function Name: lin_actuator_request_close
 *******************************************************************************
 * Summary:
 *  Queues a close request from the web interface.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void lin_actuator_request_close(void)
{
    lin_ctx.pending_request = LIN_REQUEST_CLOSE;
    printf("Info: LIN: Close requested from web UI.\r\n");
}

/*******************************************************************************
 * Function Name: lin_actuator_get_state
 *******************************************************************************
 * Summary:
 *  Returns current actuator state enum.
 *******************************************************************************/
lin_actuator_state_t lin_actuator_get_state(void)
{
    return lin_ctx.state;
}

/*******************************************************************************
 * Function Name: lin_actuator_is_ready
 *******************************************************************************
 * Summary:
 *  Returns true when actuator calibration has completed.
 *******************************************************************************/
bool lin_actuator_is_ready(void)
{
    return lin_ctx.all_positions_learned;
}

/*******************************************************************************
 * Function Name: lin_actuator_get_state_string
 *******************************************************************************
 * Summary:
 *  Returns the readable state string.
 *******************************************************************************/
const char* lin_actuator_get_state_string(void)
{
    return lin_state_to_string(lin_ctx.state);
}

/*******************************************************************************
 * Function Name: lin_actuator_get_last_debug_string
 *******************************************************************************
 * Summary:
 *  Returns the most recent debug text.
 *******************************************************************************/
const char* lin_actuator_get_last_debug_string(void)
{
    return lin_ctx.last_debug;
}

/*******************************************************************************
 * Function Name: lin_actuator_get_http_status
 *******************************************************************************
 * Summary:
 *  Formats a short HTML-friendly status string for SSE updates.
 *
 * Parameters:
 *  buffer     - Output buffer
 *  buffer_len - Output buffer length
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void lin_actuator_get_http_status(char *buffer, size_t buffer_len)
{
    snprintf(buffer,
             buffer_len,
             "LIN State: %s <br> Learned Positions: %s <br> Last Event: %s",
             lin_state_to_string(lin_ctx.state),
             lin_ctx.all_positions_learned ? "YES" : "NO",
             lin_ctx.last_debug);
}
