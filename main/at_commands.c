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

void parse_at_command(char *cmd) {
    if (strncmp(cmd, "AT+GET=USERNAME", 15) == 0) {
        printf("USERNAME: %s\n", username);
    } else if (strncmp(cmd, "AT+SET=USERNAME,", 16) == 0) {
        strncpy(username, cmd + 16, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
        printf("USERNAME SET TO: %s\n", username);
    } else if (strncmp(cmd, "AT+GET=USER_ID", 14) == 0) {
        printf("USER_ID: %s\n", user_id);
    } else if (strncmp(cmd, "AT+SET=USER_ID,", 15) == 0) {
        strncpy(user_id, cmd + 15, sizeof(user_id) - 1);
        user_id[sizeof(user_id) - 1] = '\0';
        printf("USER_ID SET TO: %s\n", user_id);
    } else if (strncmp(cmd, "AT+GET=RECENT_UID", 17) == 0) {
        printf("RECENT_UID: %s\n", recent_uid);
    } else if (strncmp(cmd, "AT+GET=POINTS", 13) == 0) {
        printf("POINTS: %d\n", points);
    } else if (strncmp(cmd, "AT+SET=POINTS,", 14) == 0) {
        points = atoi(cmd + 14);
        printf("POINTS SET TO: %d\n", points);
    } else if (strncmp(cmd, "AT+GET=CAPACITY", 15) == 0) {
        printf("CAPACITY: %d\n", capacity);
    } else if (strncmp(cmd, "AT+SET=CAPACITY,", 16) == 0) {
        capacity = atoi(cmd + 16);
        printf("CAPACITY SET TO: %d\n", capacity);
    } else {
        printf("INVALID COMMAND\n");
    }
}

void uart_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            data[len] = '\0';
            parse_at_command((char *)data);
        }
    }
}