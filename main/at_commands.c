#include "driver/uart.h"
#include "main.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_NUM UART_NUM_1
#define BUF_SIZE 256
#define UART_TX_PIN 17
#define UART_RX_PIN 16

void uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
}

void uart_send_response(const char *response) {
    // Send response to UART
    uart_write_bytes(UART_NUM, response, strlen(response));
    uart_write_bytes(UART_NUM, "\r\n", 1);  // Ensure proper AT response format
}

void parse_at_command(char *cmd) {
    char response[64];  // Buffer for response messages

    if (strncmp(cmd, "AT+GET=USERNAME", 15) == 0) {
        snprintf(response, sizeof(response), "USERNAME: %s", username);
    } else if (strncmp(cmd, "AT+SET=USERNAME,", 16) == 0) {
        strncpy(username, cmd + 16, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
        snprintf(response, sizeof(response), "USERNAME SET TO: %s", username);
    } else if (strncmp(cmd, "AT+GET=USER_ID", 14) == 0) {
        snprintf(response, sizeof(response), "USER_ID: %s", user_id);
    } else if (strncmp(cmd, "AT+SET=USER_ID,", 15) == 0) {
        strncpy(user_id, cmd + 15, sizeof(user_id) - 1);
        user_id[sizeof(user_id) - 1] = '\0';
        snprintf(response, sizeof(response), "USER_ID SET TO: %s", user_id);
    } else if (strncmp(cmd, "AT+GET=RECENT_UID", 17) == 0) {
        snprintf(response, sizeof(response), "RECENT_UID: %s", recent_uid);
    } else if (strncmp(cmd, "AT+GET=POINTS", 13) == 0) {
        snprintf(response, sizeof(response), "POINTS: %d", points);
    } else if (strncmp(cmd, "AT+SET=POINTS,", 14) == 0) {
        points = atoi(cmd + 14);
        snprintf(response, sizeof(response), "POINTS SET TO: %d", points);
    } else if (strncmp(cmd, "AT+GET=CAPACITY", 15) == 0) {
        snprintf(response, sizeof(response), "CAPACITY: %d", capacity);
    } else if (strncmp(cmd, "AT+SET=CAPACITY,", 16) == 0) {
        capacity = atoi(cmd + 16);
        snprintf(response, sizeof(response), "CAPACITY SET TO: %d", capacity);
    } else {
        snprintf(response, sizeof(response), "INVALID COMMAND");
    }

    // Print response to console
    printf("%s\n", response);

    // Send response over UART
    uart_send_response(response);
}

void uart_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            data[len] = '\0';
            printf("received uart: %s\n", data);
            parse_at_command((char *)data);
        }
    }
}