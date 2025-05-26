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

// for light
const struct device *clk_dev;
const struct device *data_dev;
int clk_pin;
int data_pin;

#define LED_CLK_NODE DT_ALIAS(rgbclk)
#define LED_DATA_NODE DT_ALIAS(rgbdata)


// Define RGB values for the 8 primary colours
// colours set by R, G, B, intensity (as on-255 or off-0)
static const uint8_t COLOURS[][3] = {
    {0, 0, 0},      // Black
    {0, 0, 255},    // Blue
    {0, 255, 0},    // Green
    {0, 255, 255},  // Cyan
    {255, 0, 0},    // Red
    {255, 0, 255},  // Magenta
    {255, 255, 0},  // Yellow
    {255, 255, 255} // White
};



uint16_t temp;
uint16_t clap;


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

    // Unpack values

    temp = (payload[MAJOR_OFFSET] << 8) | payload[MAJOR_OFFSET + 1];
    clap = payload[MINOR_OFFSET + 1];


    printk("Received - Temp: %d, Clap: %d\n", temp, clap);


    return false;  // Stop parsing further
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi,
                    uint8_t adv_type, struct net_buf_simple *ad) {
    //printk("Device found with RSSI %d\n", rssi);
    bt_data_parse(ad, adv_data_cb, (void *)addr);
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

void reset_leds(void) {
    gpio_pin_set(data_dev, data_pin, 0);
    k_msleep(100); // hold low to reset internal state
}

// Toggle between OFF (black) and ON (white)
void toggle_light(void) {
    static bool light_on = false;

    if (light_on) {
        send_colour(0, 0, 0);  // Off (black)
        printk("Toggled LED OFF\n");
    } else {
        send_colour(255, 255, 255);  // On (white)
        printk("Toggled LED ON\n");
    }

    light_on = !light_on;
}



// debug
void light_off(void) {

    int index = 0;
    
    //send_colour(0, 0, 0);  // Off (black)
    
    
    printk("Sending colour: R=%d, G=%d, B=%d\n", COLOURS[index][0], COLOURS[index][1], COLOURS[index][2]);

    // call function to send colour data to LED
    send_colour(COLOURS[index][0], COLOURS[index][1], COLOURS[index][2]);
    
}

int main(void)
{
    printk("Starting base node...\n");
    int err = bt_enable(NULL);


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

    // Get GPIO devices from device tree
    // gets handle to GPIO controller defined in overlay file
    clk_dev = DEVICE_DT_GET(DT_GPIO_CTLR(LED_CLK_NODE, gpios));
    data_dev = DEVICE_DT_GET(DT_GPIO_CTLR(LED_DATA_NODE, gpios));
    // gets pin number from LED clock and data - reads macros from overlay at runtime
    clk_pin = DT_GPIO_PIN(LED_CLK_NODE, gpios);
    data_pin = DT_GPIO_PIN(LED_DATA_NODE, gpios);



    // check if GPIO driver for clk and data is initialised
    if (!device_is_ready(clk_dev) || !device_is_ready(data_dev)) {
        printk("Error: LED GPIO devices not ready\n"); // debug output
        return -1;
    } else {
        printk("Chainable LED initialised");
    }

    // Configure GPIO pins for output - set high by default
    gpio_pin_configure(clk_dev, clk_pin, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure(data_dev, data_pin, GPIO_OUTPUT_ACTIVE);

   

    reset_leds();

    // init time
    //uint64_t last_toggle_time = 0;

    while (1) {
        //k_msleep(20);


        bt_le_scan_stop();
        // //print_to_serial();
        nanopb_encode_and_print();
        nanopb_decode_and_print(test_encoded_buffer, test_encoded_len);
        //if clap, switch light
        if (clap == 1) {
             toggle_light();
             clap = 0;  // reset after action
        }

        // uint64_t now = k_uptime_get();

        // //Toggle light every 1000 ms
        // if (now - last_toggle_time >= 1000) {
        //     toggle_light();  // Toggle the LED
        //     last_toggle_time = now;
        // }
    
        bt_le_scan_start(&scan_params, device_found);

         
        

    }

    return 0;
}
