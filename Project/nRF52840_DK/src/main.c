#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/version.h>
#include <zephyr/logging/log.h>
#include <zephyr/data/json.h>
#include <stdlib.h>
#include <zephyr/drivers/uart.h>
#include <ctype.h>
#include <zephyr/drivers/sensor.h>
#include "sensor_data.pb.h"
#include <pb_encode.h>
#include <pb_decode.h>



#define MAJOR_OFFSET 20
#define MINOR_OFFSET 22

uint16_t temp;
uint16_t light;
uint16_t clap;
bool new_temp;
bool new_light;
bool new_clap;

struct json_data {
    int temp;
    int light;
    int clap;
};

static uint8_t test_encoded_buffer[64];
static size_t test_encoded_len;


static const struct json_obj_descr data_struct[] = {
    JSON_OBJ_DESCR_PRIM(struct json_data, temp, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct json_data, light, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct json_data, clap, JSON_TOK_NUMBER),
};


extern void json_print(int major, int value) {
    char json_output[256];

    struct json_data data;
    data.temp = temp;
    data.light = light;
    data.clap = clap;

    if (clap) {
        //Reset clap for next time
        clap = 0;
    }

    int ret = json_obj_encode_buf(data_struct, ARRAY_SIZE(data_struct),
                                  &data, json_output, sizeof(json_output));
    
    if (ret < 0) {
        printk("Encoding failed with error code: %d", ret);
    }
    printk("%s\r\n", json_output);
}

// void nanopb_encode_and_print(void) {
//     SensorData data = SensorData_init_zero;
//     data.temp = temp;
//     data.light = light;
//     data.clap = clap;

//     uint8_t buffer[64];
//     pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

//     if (!pb_encode(&stream, SensorData_fields, &data)) {
//         printk("Nanopb encoding failed: %s\n", PB_GET_ERROR(&stream));
//         return;
//     }

//     size_t len = stream.bytes_written;
//     printk("Nanopb Encoded SensorData (%d bytes): ", len);
//     for (size_t i = 0; i < len; i++) {
//         printk("%02X ", buffer[i]);
//     }
//     printk("\n");
// }

void nanopb_encode_and_print(void) {
    SensorData data = SensorData_init_zero;
    data.temp = temp;
    data.light = light;
    data.clap = clap;

    pb_ostream_t stream = pb_ostream_from_buffer(test_encoded_buffer, sizeof(test_encoded_buffer));

    if (!pb_encode(&stream, SensorData_fields, &data)) {
        printk("Nanopb encoding failed: %s\n", PB_GET_ERROR(&stream));
        return;
    }

    test_encoded_len = stream.bytes_written;
    printk("Nanopb Encoded SensorData (%d bytes): ", test_encoded_len);
    for (size_t i = 0; i < test_encoded_len; i++) {
        printk("%02X ", test_encoded_buffer[i]);
    }
    printk("\n");
}

void nanopb_decode_and_print(const uint8_t *buffer, size_t len) {
    SensorData data = SensorData_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(buffer, len);

    if (!pb_decode(&stream, SensorData_fields, &data)) {
        printk("Nanopb decoding failed: %s\n", PB_GET_ERROR(&stream));
        return;
    }

    printk("Decoded SensorData:\n");
    printk("  Temp: %d\n", data.temp);
    printk("  Light: %d\n", data.light);
    printk("  Clap: %d\n", data.clap);
}



static bool adv_data_cb(struct bt_data *data, void *user_data) {


    static const uint8_t expected_uuid[16] = {
        0x19, 0xEE, 0x15, 0x16, 0x01, 0x6B, 0x4B, 0xEC,
        0xAD, 0x96, 0xBC, 0xB9, 0x6D, 0x16, 0x6E, 0x97
    };

    // Only parse manufacturer-specific data
    if (data->type != BT_DATA_MANUFACTURER_DATA || data->data_len < 24) return true;

    const uint8_t *payload = data->data;

    // Check for iBeacon header
    if (!(payload[0] == 0x4C && payload[1] == 0x00 && payload[2] == 0x02 && payload[3] == 0x15)) return true;

    // Check UUID match
    if (memcmp(&payload[4], expected_uuid, 16) != 0) return true;

    //printk("Advertisement received, type: %d, len: %d\n", data->type, data->data_len);

    
    // Extract fields
    //uint16_t major = (payload[20] << 8) | payload[20 + 1];
    //uint16_t minor = (payload[22] << 8) | payload[22 + 1];


    // Unpack values
    // temp  = (major >> 8) & 0xFF;
    // light = major & 0xFF;
    // clap  = (minor >> 8) & 0xFF;
    temp = (payload[MAJOR_OFFSET] << 8) | payload[MAJOR_OFFSET + 1];
    clap = (payload[MINOR_OFFSET] << 8) | payload[MINOR_OFFSET + 1];

    new_temp  = true;
    new_light = true;
    new_clap  = true;

    printk("Received - Temp: %d, Light: %d, Clap: %d\n", temp, light, clap);


    return false;  // Stop parsing further
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi,
                    uint8_t adv_type, struct net_buf_simple *ad) {
    //printk("Device found with RSSI %d\n", rssi);
    bt_data_parse(ad, adv_data_cb, (void *)addr);
}

int main(void)
{
    int err = bt_enable(NULL);

    
    printk("Starting base node...\n");

    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }


    static const struct bt_le_scan_param scan_params = {
        .type     = BT_LE_SCAN_TYPE_ACTIVE,
        .options  = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
        .interval = 0x0060,
        .window   = 0x0030,
    };

    err = bt_le_scan_start(&scan_params, device_found);
    if (err) {
        printk("Scan start failed (err %d)\n", err);
    } else {
        printk("BLE scanning started...\n");
    }



    while (1) {
        k_msleep(20);
        
    
        if (new_temp && new_light) {
            bt_le_scan_stop();
            //print_to_serial();
            nanopb_encode_and_print();
            nanopb_decode_and_print(test_encoded_buffer, test_encoded_len);
            
            new_temp = false;
            new_light = false;
            bt_le_scan_start(&scan_params, device_found);
        }
        if (new_clap) {
            bt_le_scan_stop();
            //print_to_serial();
            nanopb_encode_and_print();
            nanopb_decode_and_print(test_encoded_buffer, test_encoded_len);
            new_clap = false;
            bt_le_scan_start(&scan_params, device_found);
        }
    }

    return 0;
}
