#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>
#include <math.h>
#include "driver/uart.h"
#include "main.h"
#include "at_commands.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *TAG = "BLE_PERIPHERAL";
static uint8_t ble_addr_type;

static int ble_gap_event(struct ble_gap_event *event, void *arg);
void my_task(void *pvParameters);

// Define GATT Service and Characteristics UUIDs
// 05456b71-b436-4c17-8d21-fd7fad72d8fd
// 82d840b4-6ffd-4717-b7cb-590433197b66
// c0e514f4-ff78-4226-adbe-93fb0b73473e
// 9402c04c-57dd-4b1b-9add-95e3e1528483
// a9c5f3f0-46ef-4cbe-8705-3c4f2db23aa8
// 16e954e6-2f74-44eb-94c9-f0643b31befa
// ea60b997-074f-4aaa-84a6-b3d57edc7b6d
static const ble_uuid128_t gatt_service_uuid = 
    BLE_UUID128_INIT(0xfd, 0xd8, 0x72, 0xad, 0x7f, 0xfd, 0x21, 0x8d, 0x17, 0x4c, 0x36, 0xb4, 0x71, 0x6b, 0x45, 0x05);
static const ble_uuid128_t gatt_service_uuid_data_available = 
    BLE_UUID128_INIT(0x66, 0x7B, 0x19, 0x33, 0x04, 0x59, 0xCB, 0xB7, 0x17, 0x47, 0xFD, 0x6F, 0xB4, 0x40, 0xD8, 0x82);

static const ble_uuid128_t username_uuid = 
    BLE_UUID128_INIT(0x3e, 0x47, 0x73, 0x0b, 0xfb, 0x93, 0xbe, 0xad, 0x26, 0x42, 0x78, 0xff, 0xf4, 0x14, 0xe5, 0xc0);
static const ble_uuid128_t user_id_uuid = 
    BLE_UUID128_INIT(0x83, 0x84, 0x52, 0xe1, 0xe3, 0x95, 0xdd, 0x9a, 0x1b, 0x4b, 0xdd, 0x57, 0x4c, 0xc0, 0x02, 0x94);
static const ble_uuid128_t recent_uid_uuid = 
    BLE_UUID128_INIT(0xa8, 0x3a, 0xb2, 0x2d, 0x4f, 0x3c, 0x05, 0x87, 0xbe, 0x4c, 0xef, 0x46, 0xf0, 0xf3, 0xc5, 0xa9);
static const ble_uuid128_t capacity_uuid = 
    BLE_UUID128_INIT(0xfa, 0xb3, 0x31, 0x3b, 0x64, 0xf0, 0xc9, 0x94, 0xeb, 0x44, 0x74, 0x2f, 0xe6, 0x54, 0xe9, 0x16);
static const ble_uuid128_t points_uuid = 
    BLE_UUID128_INIT(0x6d, 0x7b, 0xdc, 0x7e, 0xd5, 0xb3, 0xa6, 0x84, 0xaa, 0x4a, 0x4f, 0x07, 0x97, 0xb9, 0x60, 0xea);

char username[32] = "";
char user_id[32] = "";
char recent_uid[32] = "";
int points = 0;
int capacity = 100;
bool data_available = false;

static int write_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    char *target = (char *)arg;
    int len = MIN(ctxt->om->om_len, sizeof(username) - 1);
    memcpy(target, ctxt->om->om_data, len);
    target[len] = '\0';
    ESP_LOGI(TAG, "Received write: %s", target);
    return 0;
}

static int read_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    os_mbuf_append(ctxt->om, arg, strlen((char *)arg));
    return 0;
}

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &username_uuid.u,
                .access_cb = write_handler,
                .flags = BLE_GATT_CHR_F_WRITE,
                .arg = username
            },
            {
                .uuid = &user_id_uuid.u,
                .access_cb = write_handler,
                .flags = BLE_GATT_CHR_F_WRITE,
                .arg = user_id
            },
            {
                .uuid = &recent_uid_uuid.u,
                .access_cb = read_handler,
                .flags = BLE_GATT_CHR_F_READ,
                .arg = recent_uid
            },
            {
                .uuid = &capacity_uuid.u,
                .access_cb = read_handler,
                .flags = BLE_GATT_CHR_F_READ,
                .arg = &capacity
            },
            {
                .uuid = &points_uuid.u,
                .access_cb = read_handler,
                .flags = BLE_GATT_CHR_F_READ,
                .arg = &points
            },
            {0} // End of characteristics
        }
    },
    {0} // End of services
};

// Advertise BLE Service
static void ble_advertise(void) {
    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };

    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    if(data_available) {
        fields.uuids128 = (ble_uuid128_t*)&gatt_service_uuid_data_available;
    } else {
        fields.uuids128 = (ble_uuid128_t*)&gatt_service_uuid;
    }
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    ble_gap_adv_set_fields(&fields);
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    ESP_LOGI(TAG, "Started Advertising");
}

static void ble_on_sync(void) {
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_advertise();
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connected");
            } else {
                ESP_LOGI(TAG, "Connection failed, restarting advertisement");
                ble_advertise();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected, restarting advertisement");
            strncpy(recent_uid, user_id, sizeof(recent_uid) - 1);
            recent_uid[sizeof(recent_uid) - 1] = '\0';            
            ble_advertise();
            break;
        default:
            break;
    }
    return 0;
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting BLE Peripheral...");
    nvs_flash_init();  // Initialize NVS flash storage
    nimble_port_init();
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;

    ble_svc_gap_device_name_set("esp32");
    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(gatt_services);
    ble_gatts_add_svcs(gatt_services);
    nimble_port_freertos_init(nimble_port_run);
    xTaskCreate(my_task, "my task", 4096, NULL, 3, NULL);
}

void my_task(void *pvParameters) {
    uart_init();
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 2, NULL);
    while(1)
    {
        printf("my task is running!\n");
        data_available = !data_available;
        ble_advertise();
        printf("switch uuid\n");
        printf("username: %s, uid: %s, target_uid: %s, points: %d, capacity: %d\n", username, user_id, recent_uid, points, capacity);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
