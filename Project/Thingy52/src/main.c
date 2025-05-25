/*
Clap detection using Thingy52 PDM mic
*/

#include <stdlib.h>
#include <math.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(dmic_sample);

#define MAX_SAMPLE_RATE  16000
#define SAMPLE_BIT_WIDTH 16
#define BYTES_PER_SAMPLE sizeof(int16_t)
/* Milliseconds to wait for a block to be read. */
#define READ_TIMEOUT     1000

// alias'
#define HTS221_NODE DT_ALIAS(temphum)
const struct device *hts221_dev = DEVICE_DT_GET(HTS221_NODE);

#define LIGHT_POWER_PIN 8  // Adjust based on SX1509B pin
const struct device *expander = DEVICE_DT_GET(DT_NODELABEL(sx1509b));
const struct device *light_sensor = DEVICE_DT_GET(DT_ALIAS(light));


/* Size of a block for 100 ms of audio data. */
#define BLOCK_SIZE(_sample_rate, _number_of_channels) \
	(BYTES_PER_SAMPLE * (_sample_rate / 10) * _number_of_channels)

/* Driver will allocate blocks from this slab to receive audio data into them.
 * Application, after getting a given block from the driver and processing its
 * data, needs to free that block.
 */
#define MAX_BLOCK_SIZE   BLOCK_SIZE(MAX_SAMPLE_RATE, 2)
#define BLOCK_COUNT      4
K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, BLOCK_COUNT, 4);

#define LED0_NODE DT_ALIAS(led0)
// Set up LED for debugging
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Constants for BLE
#define UUID_OFFSET 4
#define MAJOR_OFFSET 20
#define MINOR_OFFSET 22
#define TX_POWER_OFFSET 24
#define IBEACON_EXPECTED_LEN 25
#define ORIGINAL_RSSI_OFFSET 25

struct values {
	uint16_t temp;
	uint16_t light;
	uint16_t clap;
	uint16_t newtemp;
	uint16_t newlight;
	uint16_t newclap;
};

static int do_pdm_transfer(const struct device *dmic_dev,
			   struct dmic_cfg *cfg,
			   size_t block_count,
			   struct values *data)
{
	int ret;

	ret = dmic_configure(dmic_dev, cfg);
	if (ret < 0) {
		LOG_ERR("Failed to configure the driver: %d", ret);
		return ret;
	}

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
	if (ret < 0) {
		LOG_ERR("START trigger failed: %d", ret);
		return ret;
	}

	for (int i = 0; i < block_count; ++i) {
		void *buffer;
		uint32_t size;
		
		// Toggle LED for debugging
		gpio_pin_toggle_dt(&led);

		// Read PDM values
		ret = dmic_read(dmic_dev, 0, &buffer, &size, READ_TIMEOUT);
		if (ret < 0) {
			LOG_ERR("%d - read failed: %d", i, ret);
			return ret;
		}

		// Toggle LED for debugging 
		gpio_pin_toggle_dt(&led);

		// Convert PDM values to PCM values
		int16_t *samples = (int16_t *)buffer;
		size_t sample_count = size / sizeof(int16_t);

		// Detect clap
		for (size_t i = 0; i < sample_count; ++i) {
			if (abs(samples[i]) > 10000) {
				//LOG_INF("Clap at sample %d", i);
				// update clap
				data->clap += 1;
				data->newclap = 1;
			}
		}

		// Provide time for memory to be freed
		k_msleep(100);

		k_mem_slab_free(&mem_slab, buffer);
	}

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);
	if (ret < 0) {
		LOG_ERR("STOP trigger failed: %d", ret);
		return ret;
	}

	return ret;
}

void read_temperature(struct values *data)
{
	struct sensor_value temp_val;

	if (!device_is_ready(hts221_dev)) {
		LOG_ERR("HTS221 temperature sensor not ready");
		return;
	}

	if (sensor_sample_fetch(hts221_dev) == 0 &&
		sensor_channel_get(hts221_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_val) == 0) {

		data->temp = (uint16_t)(sensor_value_to_double(&temp_val) * 100);
		data->newtemp = 1;
	}
}

void power_light_sensor(void) {
    if (!device_is_ready(expander)) {
        printk("SX1509B not ready\n");
        return;
    }
    gpio_pin_configure(expander, LIGHT_POWER_PIN, GPIO_OUTPUT_ACTIVE);
    gpio_pin_set(expander, LIGHT_POWER_PIN, 1);
    k_msleep(500);  // Wait for sensor to stabilize
}

void read_light_intensity(void) {
    if (!device_is_ready(light_sensor)) {
        printk("APDS9960 not ready\n");
        return;
    }

    struct sensor_value light;
    if (sensor_sample_fetch(light_sensor) < 0) {
        printk("Failed to fetch sample\n");
        return;
    }

    if (sensor_channel_get(light_sensor, SENSOR_CHAN_LIGHT, &light) < 0) {
        printk("Failed to read light channel\n");
        return;
    }

    printk("Ambient Light: %d\n", light.val1);
}




static void advertise_thingy(struct values data) {
    uint8_t beacon_data[] = {
        0x4C, 0x00, 0x02, 0x15,                         // Apple iBeacon prefix
        0x19, 0xEE, 0x15, 0x16, 0x01, 0x6B, 0x4B, 0xEC, // UUID part 1 (example)
        0xAD, 0x96, 0xBC, 0xB9, 0x6D, 0x16, 0x6E, 0x97, // UUID part 2 (example)
        0x00, 0x00, 0x00, 0x00,                         // Major / Minor placeholder
        0xC8                                            // TX Power (example value)
    };

	// Pack values: top byte = temp, bottom byte = light
    //uint16_t major_part = ((data.temp & 0xFF) << 8) | (data.light & 0xFF);
    // Clap into upper byte of minor, lower byte can be a flag (0x01) if needed
    //uint16_t minor_part = ((data.clap & 0xFF) << 8) | 0x01;


	printk("Packing BLE Data - Temp: %d, Light: %d, Clap: %d", data.temp, data.light, data.clap);

    //sys_put_be16(major_part, &beacon_data[MAJOR_OFFSET]);
    //sys_put_be16(minor_part, &beacon_data[MINOR_OFFSET]);
	sys_put_be16(data.temp, &beacon_data[MAJOR_OFFSET]);
	sys_put_be16(data.clap, &beacon_data[MINOR_OFFSET]);

    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, beacon_data, sizeof(beacon_data)),
    };

    bt_le_adv_stop();
    int err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);

	if (err) {
		printk("Advertising failed: %d\n", err);
	} else {
		printk("[ADVERTISING] iBeacon Payload (%d bytes): ", sizeof(beacon_data));
		for (int i = 0; i < sizeof(beacon_data); i++) {
			printk("%02X ", beacon_data[i]);
		}
		printk("\n");
	}

    // Also show decoded values for clarity
    // printk("Decoded - Temp: %d, Light: %d, Clap: %d (Major: 0x%04X, Minor: 0x%04X)\n",
    //        data.temp, data.light, data.clap, major_part, minor_part);

    k_sleep(K_MSEC(20));
    bt_le_adv_stop();
}



// checks for connected devices by scanning the I2C bus for addresses that respond
void i2c_scan(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c_dev)) {
        printk("I2C device not ready\n");
        return;
    }

    printk("Scanning I2C bus...\n");
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        uint8_t dummy = 0;
        struct i2c_msg msgs[] = {
            {
                .buf = &dummy,
                .len = 1,
                .flags = I2C_MSG_WRITE | I2C_MSG_STOP,
            },
        };
        if (i2c_transfer(i2c_dev, msgs, 1, addr) == 0) {
            printk("Found device at address: 0x%02X\n", addr);
        }
    }
}

// configures the SX1509B GPIO expander to power the APDS9960 sensor
void power_apds9960_via_expander(void)
{
    if (!device_is_ready(expander)) {
        printk("SX1509B expander not ready\n");
        return;
    }

    int ret = gpio_pin_configure(expander, LIGHT_POWER_PIN, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Failed to configure expander pin %d\n", LIGHT_POWER_PIN);
        return;
    }

    gpio_pin_set(expander, LIGHT_POWER_PIN, 1); // Power on
    printk("Powered APDS9960 using SX1509B pin %d\n", LIGHT_POWER_PIN);

    k_msleep(500); // Allow sensor to power up
}


int main(void)
{
	k_sleep(K_SECONDS(3));  // Wait for RTT Viewer to connect

	const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));
	int ret;

	// Initialise Bluetooth
    int err;
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed: %d\n", err);
        return 0;
    }
    printk("Bluetooth initialized\n");

	LOG_INF("DMIC sample");

	if (!device_is_ready(dmic_dev)) {
		LOG_ERR("%s is not ready", dmic_dev->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	// checks temp sensor readiness
	if (!device_is_ready(hts221_dev)) {
	LOG_ERR("HTS221 temperature sensor not ready");
	return 0;
	}

	power_light_sensor();
	power_apds9960_via_expander();
	i2c_scan();

	// Try to read APDS9960 ID from I2C address 0x68
	uint8_t id = 0;
	const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	if (i2c_reg_read_byte(i2c_dev, 0x68, 0x92, &id) == 0) {
		printk("APDS9960 ID read from 0x68 reg 0x92: 0x%02X\n", id);
	} else {
		printk("Failed to read APDS9960 ID from 0x68\n");
	}

	// for (int i = 0; i < 16; i++) {
	// 	printk("Testing pin %d\n", i);
	// 	gpio_pin_configure(expander, i, GPIO_OUTPUT_ACTIVE);
	// 	gpio_pin_set(expander, i, 1);
	// 	k_msleep(1000);  // give time to see if sensor appears

	// 	i2c_scan(); // or manually try reading reg 0x92 from APDS9960
	// 	uint8_t id;
	// 	const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	// 	if (i2c_reg_read_byte(i2c_dev, 0x39, 0x92, &id) == 0) {
	// 		printk("Read APDS9960 ID: 0x%02X from SX1509B pin %d\n", id, i);
	// 	} else {
	// 		printk("Failed to read APDS9960 ID on SX1509B pin %d\n", i);
	// 	}

	// 	gpio_pin_set(expander, i, 0);  // Turn off pin after test
	// 	k_msleep(500);
	// }
	// Try to read register 0x92 from address 0x68

	

	struct pcm_stream_cfg stream = {
		.pcm_width = SAMPLE_BIT_WIDTH,
		.mem_slab  = &mem_slab,
	};
	struct dmic_cfg cfg = {
		.io = {
			/* These fields can be used to limit the PDM clock
			 * configurations that the driver is allowed to use
			 * to those supported by the microphone.
			*/
			.min_pdm_clk_freq = 1000000,
			.max_pdm_clk_freq = 3250000,
			.min_pdm_clk_dc   = 40,
			.max_pdm_clk_dc   = 60,
		},
		.streams = &stream,
		.channel = {
			.req_num_streams = 1,
		},
	};

	cfg.channel.req_num_chan = 1;
	cfg.channel.req_chan_map_lo =
		dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
	cfg.streams[0].pcm_rate = MAX_SAMPLE_RATE;
	cfg.streams[0].block_size =
		BLOCK_SIZE(cfg.streams[0].pcm_rate, cfg.channel.req_num_chan);

	while (1) {
		
		//read_light_intensity();

		// gpio_pin_set(expander, LIGHT_POWER_PIN, 0);
		// k_msleep(200);
		// gpio_pin_set(expander, LIGHT_POWER_PIN, 1);
		// printk("Toggled SX1509B pin %d\n", LIGHT_POWER_PIN);

		struct values data = {0}; // Reset sensor values

		// Read temperature and light into struct
		read_temperature(&data);
		//read_light(&data);
		// Detect claps and update struct
		ret = do_pdm_transfer(dmic_dev, &cfg, BLOCK_COUNT, &data);
		if (ret < 0) {
			return 0;
		}

		// Send over BLE
		advertise_thingy(data);

		k_sleep(K_SECONDS(5));
	}

	LOG_INF("Exiting");
	return 0;
}
