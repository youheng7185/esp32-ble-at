#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>
#include <math.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *TAG = "BLE_PERIPHERAL";
static uint8_t ble_addr_type;

static int ble_gap_event(struct ble_gap_event *event, void *arg);

// Define GATT Service and Characteristics UUIDs
static const ble_uuid128_t gatt_service_uuid = 
    BLE_UUID128_INIT(0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0);
static const ble_uuid128_t username_uuid = 
    BLE_UUID128_INIT(0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01);
static const ble_uuid128_t user_id_uuid = 
    BLE_UUID128_INIT(0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x02);
static const ble_uuid128_t recent_uid_uuid = 
    BLE_UUID128_INIT(0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x03);
static const ble_uuid128_t capacity_uuid = 
    BLE_UUID128_INIT(0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x04);
static const ble_uuid128_t points_uuid = 
    BLE_UUID128_INIT(0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x05);

static char username[32] = "";
static char user_id[32] = "";
static char recent_uid[32] = "";
static int points = 0;
static int capacity = 100;

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
    fields.uuids128 = (ble_uuid128_t*)&gatt_service_uuid;
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
}
