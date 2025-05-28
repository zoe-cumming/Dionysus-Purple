#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
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
#define TX_POWER_OFFSET 24
#define IBEACON_EXPECTED_LEN 25

// for light
const struct device *clk_dev;
const struct device *data_dev;
int clk_pin;
int data_pin;

#define LED_CLK_NODE DT_ALIAS(rgbclk)
#define LED_DATA_NODE DT_ALIAS(rgbdata)

uint16_t temp;
uint16_t clap;
uint16_t light;

struct json_data {
    int temp;
    int light;
    int clap;
};

static bool light_on = false;
static bool sensors_on = true;

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

void nanopb_encode_and_print(void) {
    SensorData data = SensorData_init_zero;
    data.temp = temp;
    data.clap = light_on;
    data.light = light;

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
    printk("  Light on: %d\n", data.clap);
    printk("  Light: %d\n", data.light);
}

static bool adv_data_cb(struct bt_data *data, void *user_data) {

    static const uint8_t expected_uuid[16] = {
        0x19, 0xEE, 0x15, 0x16, 0x01, 0x6B, 0x4B, 0xEC,
        0xAD, 0x96, 0xBC, 0xB9, 0x6D, 0x16, 0x6E, 0x97
    };

    // Only parse manufacturer-specific data
    if (data->type != BT_DATA_MANUFACTURER_DATA || data->data_len < IBEACON_EXPECTED_LEN) return true;

    const uint8_t *payload = data->data;

    // Check for iBeacon header
    if (!(payload[0] == 0x4C && payload[1] == 0x00 && payload[2] == 0x02 && payload[3] == 0x15)) return true;

    // Check UUID match
    if (memcmp(&payload[4], expected_uuid, 16) != 0) return true;

    // Unpack values
    temp = (payload[MAJOR_OFFSET] << 8) | payload[MAJOR_OFFSET + 1];
    clap = payload[MINOR_OFFSET + 1];
    light = (int8_t)payload[TX_POWER_OFFSET] * 100;

    printk("Received - Temp: %d, Clap: %d, Light: %d\n", temp, clap, light);
    return false;  // Stop parsing further
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi,
                    uint8_t adv_type, struct net_buf_simple *ad) {
    //printk("Device found with RSSI %d\n", rssi);
    bt_data_parse(ad, adv_data_cb, (void *)addr);
}

// send cli command to thingy
// static void advertise_nrf(const struct shell *sh, bool sensor_state) {  
//     uint8_t beacon_data[] = {
//         0x4C, 0x00, 0x02, 0x15,                         // Apple iBeacon prefix
//         0x18, 0xEE, 0x15, 0x16, 0x01, 0x6B, 0x4B, 0xEC, // UUID part 1
//         0xAD, 0x96, 0xBC, 0xB9, 0x6D, 0x16, 0x6E, 0x97, // UUID part 2
//         0x00, 0x00, 0x00, 0x00,                         // Major / Minor placeholder
//         0xC8                                            // TX Power 
//     };

//     shell_print(sh, "Packing BLE Data - sensors setting: %d", sensor_state);

// 	// printk("Packing BLE Data - sensors setting: %d, \n",
//     //    sensor_state); 

// 	beacon_data[MAJOR_OFFSET]     = 0x00;  // MSB
//     beacon_data[MAJOR_OFFSET + 1] = (uint8_t)sensor_state;  // LSB
	

//     struct bt_data ad[] = {
//         BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
//         BT_DATA(BT_DATA_MANUFACTURER_DATA, beacon_data, sizeof(beacon_data)),
//     };

//     bt_le_adv_stop();
//     int err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);

// 	if (err) {
//         shell_print(sh, "Advertising failed: %d", err);
//     } else {
//         shell_print(sh, "[ADVERTISING] iBeacon Payload (%d bytes):", sizeof(beacon_data));
//         for (int i = 0; i < sizeof(beacon_data); i++) {
//             shell_fprintf(sh, SHELL_NORMAL, "%02X ", beacon_data[i]);
//         }
//         shell_print(sh, "");
//     }

//     k_sleep(K_MSEC(20));
//     bt_le_adv_stop();
// }

static void advertise_nrf(bool sensor_state) {  
    uint8_t beacon_data[] = {
        0x4C, 0x00, 0x02, 0x15,                         // Apple iBeacon prefix
        0x18, 0xEE, 0x15, 0x16, 0x01, 0x6B, 0x4B, 0xEC, // UUID part 1
        0xAD, 0x96, 0xBC, 0xB9, 0x6D, 0x16, 0x6E, 0x97, // UUID part 2
        0x00, 0x00, 0x00, 0x00,                         // Major / Minor placeholder
        0xC8                                            // TX Power 
    };

    shell_print(sh, "Packing BLE Data - sensors setting: %d", sensor_state);

	printk("Packing BLE Data - sensors setting: %d, \n",
        sensor_state); 

	beacon_data[MAJOR_OFFSET]     = 0x00;  // MSB
    beacon_data[MAJOR_OFFSET + 1] = (uint8_t)sensor_state;  // LSB
	

    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, beacon_data, sizeof(beacon_data)),
    };

    bt_le_adv_stop();
    int err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);

	if (err) {
        printk("Advertising failed: %d", err);
    } else {
        printk("[ADVERTISING] iBeacon Payload (%d bytes):", sizeof(beacon_data));
        for (int i = 0; i < sizeof(beacon_data); i++) {
            shell_fprintf(sh, SHELL_NORMAL, "%02X ", beacon_data[i]);
        }
        printk("");
    }

    k_sleep(K_MSEC(20));
    bt_le_adv_stop();
}


// function to send a byte to LED via the GPIO pins
void send_byte(uint8_t b) {
    for (int i = 0; i < 8; i++) {
        gpio_pin_set(data_dev, data_pin, (b & 0x80) ? 1 : 0); // set data pin based on MSB
        k_busy_wait(500); 
        // clock pulse - delays ensure stable transition
        gpio_pin_set(clk_dev, clk_pin, 0); // pull clk low
        k_busy_wait(500);
        gpio_pin_set(clk_dev, clk_pin, 1); // set clk high
        k_busy_wait(500);
        // move next bit to MSB 
        b <<= 1;
    }
}

// function to send RGB colour value to LED
void send_colour(uint8_t red, uint8_t green, uint8_t blue) {
     // Start by sending a byte with the format "1 1 /B7 /B6 /G7 /G6 /R7 /R6"
     uint8_t prefix = 0b11000000;
     if ((blue & 0x80) == 0)     prefix|= 0b00100000;
     if ((blue & 0x40) == 0)     prefix|= 0b00010000; 
     if ((green & 0x80) == 0)    prefix|= 0b00001000;
     if ((green & 0x40) == 0)    prefix|= 0b00000100;
     if ((red & 0x80) == 0)      prefix|= 0b00000010;
     if ((red & 0x40) == 0)      prefix|= 0b00000001;
     
     // Send 4 bytes of zeros before the colour data
     for (int i = 0; i < 4; i++) {
        send_byte(0);
     }
     send_byte(prefix);
         
     // send the 3 colours (LSB first, 8-bit)
     send_byte(blue);
     send_byte(green);
     send_byte(red);

     // Send 4 bytes of zeros after the colour data
     for (int i = 0; i < 4; i++) {
        send_byte(0);
     }
}

// Toggle between OFF (black) and ON (white)
void toggle_light(void) {

    if (light_on) {
        send_colour(0, 0, 0);  // Off (black)
        printk("Toggled LED OFF\n");
    } else {
        send_colour(255, 255, 255);  // On (white)
        printk("Toggled LED ON\n");
    }

    light_on = !light_on;
}


int main(void)
{
    printk("Starting base node...\n");
    int err = bt_enable(NULL);


    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    clk_dev = DEVICE_DT_GET(DT_GPIO_CTLR(LED_CLK_NODE, gpios));
    data_dev = DEVICE_DT_GET(DT_GPIO_CTLR(LED_DATA_NODE, gpios));
    clk_pin = DT_GPIO_PIN(LED_CLK_NODE, gpios);
    data_pin = DT_GPIO_PIN(LED_DATA_NODE, gpios);


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

    if (!device_is_ready(clk_dev) || !device_is_ready(data_dev)) {
        printk("LED GPIO devices not ready\n");
        return -1;
    }

    gpio_pin_configure(clk_dev, clk_pin, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure(data_dev, data_pin, GPIO_OUTPUT_ACTIVE);

    int64_t print_time = k_uptime_get();

    while (1) {
        k_msleep(20);

        bt_le_scan_stop();
        //print_to_serial();
        if ((k_uptime_get() - print_time) >= 2000) {
            //nanopb_encode_and_print();
            // nanopb_decode_and_print(test_encoded_buffer, test_encoded_len);
            print_time = k_uptime_get();
        }
        //if clap, switch light
        if (clap == 1) {
            toggle_light();
            clap = 0;  // reset after action
        }

        //advertise_nrf(sensors_on);

        bt_le_scan_start(&scan_params, device_found);
    }

    return 0;
}

/* 
 * Shell command to turn sensors on/off
 */
static int sensors_cli(const struct shell *sh, size_t argc, char **argv)
{
    if ((argc == 3) && (0 == strcmp(argv[1], "power"))) {
        if (0 == strcmp(argv[2], "on")) {
            if (sensors_on) {
                shell_print(sh, "Sensors already on");
            } else {
                sensors_on = true;
                shell_print(sh, "Turning sensors on");
                //shell_print(sh, "Calling advertise_nrf() with state %d", sensors_on);
                //advertise_nrf(sh, sensors_on);
            }
        }
        else if (0 == strcmp(argv[2], "off")) {
            if (sensors_on) {
                sensors_on = false;
                shell_print(sh, "Turning sensors off");
                //shell_print(sh, "Calling advertise_nrf() with state %d", sensors_on);
                //advertise_nrf(sh, sensors_on);
            } else {
                shell_print(sh, "Sensors already off");
            }
        }
        else {
            shell_print(sh, "Invalid Sensor Command");
            return -1;
        }
        return 0;
    } else {
        shell_print(sh, "Invalid Sensor Command");
        return -1;
    }
    return -1;
}

/* 
 * Shell command to turn lights on/off
 */
static int lights_cli(const struct shell *sh, size_t argc, char **argv)
{
    if ((argc == 2) && (0 == strcmp(argv[1], "on"))) {
        if (!light_on) {
            toggle_light();
            shell_print(sh, "Turning lights on");
        } 
        shell_print(sh, "Lights already on");
    } else if ((argc == 2) && (0 == strcmp(argv[1], "off"))) {
        if (light_on) {
            toggle_light();
            shell_print(sh, "Turning lights off");
        }
        shell_print(sh, "Lights already off");
    } else {
        shell_print(sh, "Invalid Light Command");
        return -1;
    }
    return -1;
}

// Define shell commands for CLI
SHELL_CMD_ARG_REGISTER(sensors, NULL, "sensors power on\nsensors power off\n", sensors_cli, 3, 0);
SHELL_CMD_ARG_REGISTER(lights, NULL, "lights on\nlights off\n", lights_cli, 2, 0);