/*
 * Modified from app_trace from NRF51 SDK
 * original copyright information:
 * 
 * Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <app_uart.h>

#ifdef DEBUG_LOG_ENABLE
#include "app_uart.h"
#include "nordic_common.h"
#include "boards.h"
#include "debug_log.h"
#include "app_error.h"
#include "uart_commands.h"

#ifndef UART_TX_BUF_SIZE
    #define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#endif
#ifndef UART_RX_BUF_SIZE
    #define UART_RX_BUF_SIZE 32                           /**< UART RX buffer size. */
#endif

void uart_error_handle(app_uart_evt_t * p_event)
{
    /*if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }*/
}

void uart_event_handle(app_uart_evt_t * p_event) {
    if (p_event->evt_type == APP_UART_DATA_READY) {
        uint8_t rx_byte;
        while (app_uart_get(&rx_byte) == NRF_SUCCESS) {
            UARTCommands_ProcessChar(rx_byte);
        }
    } else {
        uart_error_handle(p_event);
    }
}

void debug_log_init(void)
{
    uint32_t err_code = NRF_SUCCESS;
    const app_uart_comm_params_t comm_params =  {
        RX_PIN_NUMBER, 
        TX_PIN_NUMBER, 
        RTS_PIN_NUMBER, 
        CTS_PIN_NUMBER, 
        APP_UART_FLOW_CONTROL_DISABLED, 
        false, 
        UART_BAUDRATE_BAUDRATE_Baud115200
    }; 
        
    APP_UART_FIFO_INIT(&comm_params, 
                       UART_RX_BUF_SIZE, 
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOW,
                       err_code);

    UNUSED_VARIABLE(err_code);
}

void debug_log_dump(uint8_t * p_buffer, uint32_t len)
{
    debug_log("\r\n");
    for (uint32_t index = 0; index <  len; index++)  {
        debug_log("0x%02X ", p_buffer[index]);
    }
    debug_log("\r\n");
}

#endif // DEBUG_LOG_ENABLE

/**
 *@}
 **/

