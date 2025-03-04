#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

#define TAG "BLE_SCAN"
uint8_t ble_addr_type;  // BLE address type

// d0f07e0c-c1cb-4eb8-b396-0cbe759ed40e
const uint8_t TARGET_UUID[16] = { 
    0x0E, 0xD4, 0x9E, 0x75, 0xBE, 0x0C, 0x96, 0xB3, 
    0xB8, 0x4E, 0xCB, 0xC1, 0x0C, 0x7E, 0xF0, 0xD0 
};


void ble_app_scan(void);  // Forward declaration
static int ble_gatt_service_discovery(uint16_t conn_handle);

// BLE event handler
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_hs_adv_fields fields;

    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:  // Device discovered
        ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);

        // Print device name
        if (fields.name_len > 0)
        {
            printf("Discovered Device: %.*s\n", fields.name_len, fields.name);
        }

        // Print 128-bit Service UUIDs
        if (fields.num_uuids128 > 0 && fields.uuids128 != NULL)
        {
            printf("Service UUID: ");
            for (int i = 15; i >= 0; i--) //reverse order
            {
                printf("%02X ", fields.uuids128->value[i]);
            }
            printf("\n");

            if (memcmp(fields.uuids128->value, TARGET_UUID, 16) == 0)
            {
                printf("target uuid found\n");
                printf("device mac address is %02X:%02X:%02X:%02X:%02X:%02X\n", event->disc.addr.val[5], event->disc.addr.val[4], event->disc.addr.val[3], event->disc.addr.val[2], event->disc.addr.val[1], event->disc.addr.val[0]);
                
                ble_gap_disc_cancel(); // cancel discovery
                ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr, 1000, NULL, ble_gap_event, NULL);
            }
        }
        break;

    case BLE_GAP_EVENT_CONNECT:
        if(event->connect.status == 0)
        {
            printf("connected to ble device\n");
            ble_gatt_service_discovery(event->connect.conn_handle);
        }
        else
        {
            printf("Failed to connect, error code: %d\n", event->connect.status);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:  // Disconnected
        printf("Disconnected from BLE device.\n");
        ble_app_scan();  // Restart scanning
        break;

    case BLE_GAP_EVENT_DISC_COMPLETE:  // Scan completed
        printf("Scan complete. Restarting scan...\n");
        ble_app_scan();  // Restart scanning
        break;

    default:
        break;
    }
    return 0;
}

static int gatt_svc_discovery_cb(uint16_t conn_handle, const struct ble_gatt_svc *service, void *arg)
{
    if (service->uuid.u.type == BLE_UUID_TYPE_16)
    {
        printf("Discovered 16-bit Service UUID: %04X\n", service->uuid.u16.value);
    }
    else if (service->uuid.u.type == BLE_UUID_TYPE_32)
    {
        printf("Discovered 32-bit Service UUID: %08lX\n", service->uuid.u32.value);
    }
    else if (service->uuid.u.type == BLE_UUID_TYPE_128)
    {
        printf("Discovered 128-bit Service UUID: ");
        for (int i = 15; i >= 0; i--)  // Reverse order for readability
        {
            printf("%02X ", service->uuid.u128.value[i]);
        }
        printf("\n");
    } else {
        printf("no service uuid found\n");
    }
    return 0;
}


/* Function to Discover GATT Services */
static int ble_gatt_service_discovery(uint16_t conn_handle)
{
    printf("Starting GATT Service Discovery...\n");
    return ble_gattc_disc_all_svcs(conn_handle, gatt_svc_discovery_cb, NULL);
}

// Function to start BLE scanning
void ble_app_scan(void)
{
    struct ble_gap_disc_params ble_gap_disc_params = {0};
    ble_gap_disc_params.filter_duplicates = 0;  // Detect all devices
    ble_gap_disc_params.passive = 0;  // 1-Passive scanning, 0-active scanning
    ble_gap_disc_params.itvl = 0;
    ble_gap_disc_params.window = 0;
    ble_gap_disc_params.filter_policy = 0;
    ble_gap_disc_params.limited = 0;

    printf("Starting BLE scan...\n");
    int rc = ble_gap_disc(ble_addr_type, BLE_HS_FOREVER, &ble_gap_disc_params, ble_gap_event, NULL);
    if (rc != 0)
    {
        printf("Error starting scan: %d\n", rc);
    }
}

// BLE stack synchronization callback
void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_app_scan();
}

// BLE host task
void host_task(void *param)
{
    nimble_port_run();
}

// Main application entry point
void app_main(void)
{
    nvs_flash_init();  // Initialize NVS flash storage

    nimble_port_init();  // Initialize NimBLE

    ble_svc_gap_device_name_set("ESP32_SCANNER");  // Set BLE device name
    ble_svc_gap_init();  // Initialize BLE GAP services

    ble_hs_cfg.sync_cb = ble_app_on_sync;  // Set BLE sync callback
    nimble_port_freertos_init(host_task);  // Start BLE host task
}
