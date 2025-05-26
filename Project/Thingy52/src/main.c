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
#define READ_TIMEOUT     1000

// alias for temp sensor
#define HTS221_NODE DT_ALIAS(temphum)
const struct device *hts221_dev = DEVICE_DT_GET(HTS221_NODE);

// Block size for DMIC
#define BLOCK_SIZE(_sample_rate, _number_of_channels) \
	(BYTES_PER_SAMPLE * (_sample_rate / 10) * _number_of_channels)

// Slabs for DMIC
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

// Constants for light sensor
#define I2C_ADDRESS 0x38
#define LIGHT_WHITE 0x56
#define LIGHT_SYS_CTRL 0x40
#define LIGHT_MODECTL1 0x41

const struct device *bh1745 = DEVICE_DT_GET(DT_NODELABEL(i2c0));

#define STACK_SIZE 2048
#define PRIORITY_CLAP 1
#define PRIORITY_TEMP 3
#define PRIORITY_BLE 3
#define PRIORITY_LIGHT 3

struct values {
	uint16_t temp;
	uint8_t clap;
	uint16_t light;
};

struct values shared_data = {0};
struct k_mutex data_lock;

K_THREAD_STACK_DEFINE(clap_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(temp_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(ble_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(light_stack, STACK_SIZE);

static struct k_thread clap_thread_data;
static struct k_thread temp_thread_data;
static struct k_thread ble_thread_data;
static struct k_thread light_thread_data;

static int do_pdm_transfer(const struct device *dmic_dev,
			   struct dmic_cfg *cfg,
			   size_t block_count)
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
			if ((abs(samples[i]) > 10000) && (i != 32)) {
				LOG_INF("Clap at sample %d", i);
				// update clap
				k_mutex_lock(&data_lock, K_FOREVER);
				if (shared_data.clap) {
					shared_data.clap = 0;
				} else {
					shared_data.clap = 1;
				}
				k_mutex_unlock(&data_lock);
				break;
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
	}
}

static void advertise_thingy(struct values data) {
    uint8_t beacon_data[] = {
        0x4C, 0x00, 0x02, 0x15,                         // Apple iBeacon prefix
        0x19, 0xEE, 0x15, 0x16, 0x01, 0x6B, 0x4B, 0xEC, // UUID part 1 (example)
        0xAD, 0x96, 0xBC, 0xB9, 0x6D, 0x16, 0x6E, 0x97, // UUID part 2 (example)
        0x00, 0x00, 0x00, 0x00,                         // Major / Minor placeholder
        0xC8                                            // TX Power (example value)
    };

	printk("Packing BLE Data - Temp: %d, Clap Detected: %d\n",
       data.temp, data.clap);

	sys_put_be16(data.temp, &beacon_data[MAJOR_OFFSET]);
	sys_put_be16(data.clap, &beacon_data[MINOR_OFFSET]);
	beacon_data[TX_POWER_OFFSET] = (uint8_t)data.light;

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

	k_mutex_lock(&data_lock, K_FOREVER);
	shared_data.clap = 0;
	k_mutex_unlock(&data_lock);

    k_sleep(K_MSEC(20));
    bt_le_adv_stop();
}

// Clap Detection Thread
void clap_thread_fn(void *dmic_dev_ptr, void *cfg_ptr, void *unused) {
    const struct device *dmic_dev = dmic_dev_ptr;
    struct dmic_cfg *cfg = cfg_ptr;

    while (1) {

        if (!(do_pdm_transfer(dmic_dev, cfg, BLOCK_COUNT) == 0)) {
			LOG_ERR("DMIC read failed");
        }

        k_msleep(10); // Sampling interval
    }
}

// Temperature Sensor Thread
void temp_thread_fn(void *arg1, void *arg2, void *arg3) {
    while (1) {
        struct values local = {0};
        read_temperature(&local);

        k_mutex_lock(&data_lock, K_FOREVER);
        shared_data.temp = local.temp;
        k_mutex_unlock(&data_lock);

        k_sleep(K_SECONDS(5));  // Sample less frequently
    }
}

// Light Sensor Thread
void light_thread_fn(void *arg1, void *arg2, void *arg3) {
	int ret;
    while (1) {
		uint8_t buf[2];
		struct values local = {0};
    	ret = i2c_burst_read(bh1745, I2C_ADDRESS, LIGHT_WHITE, buf, 2);
		if (ret) {
			LOG_ERR("Failed to read light sensor");
		}

		// Lux of white light
		local.light = (buf[1] << 8) | buf[0];
        local.light = (uint8_t)(local.light * 0.4f);
		k_mutex_lock(&data_lock, K_FOREVER);
        shared_data.light = local.light;
        k_mutex_unlock(&data_lock);
		LOG_INF("Let there be light! %u", local.light);
        k_sleep(K_SECONDS(5));  // Sample less frequently
    }
}

// BLE Advertising Thread
void ble_thread_fn(void *arg1, void *arg2, void *arg3) {
    while (1) {
        struct values copy;

        k_mutex_lock(&data_lock, K_FOREVER);
        copy = shared_data;
        k_mutex_unlock(&data_lock);

        advertise_thingy(copy);
        k_sleep(K_SECONDS(1));
    }
}


int main(void)
{
	static const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));
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

	// Manually turn on powr to mic
	const struct device *const expander = DEVICE_DT_GET(DT_NODELABEL(sx1509b));
	if (!device_is_ready(expander)) {
		LOG_ERR("%s is not ready", expander->name);
		return 0;
	}
	gpio_pin_configure(expander, 9, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set(expander, 9, 1);

	static struct pcm_stream_cfg stream = {
		.pcm_width = SAMPLE_BIT_WIDTH,
		.mem_slab  = &mem_slab,
	};

	// Configure clocks for mic
	static struct dmic_cfg cfg = {
		.io = {
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
	

	// Initialise the light sensor
	if (!device_is_ready(bh1745)) {
		LOG_ERR("%s is not ready", dmic_dev->name);
		return 0;
	}

	// Configure directly with I2C
	uint8_t buf[2];
	buf[0] = LIGHT_SYS_CTRL;
	buf[1] = 0x80;
	i2c_write(bh1745, buf, 2, I2C_ADDRESS);
	k_msleep(2);
	buf[0] = LIGHT_SYS_CTRL;
	buf[1] = 0x00;
    i2c_write(bh1745, buf, 2, I2C_ADDRESS);

	uint8_t config[] = {LIGHT_MODECTL1, 0x00,
                     0x10,
                     0x02};
    i2c_write(bh1745, config, sizeof(cfg), I2C_ADDRESS);

	k_mutex_init(&data_lock);

	// Start threads
	k_thread_create(&clap_thread_data, clap_stack, STACK_SIZE,
					clap_thread_fn, (void *)dmic_dev, (void *)&cfg, NULL,
					PRIORITY_CLAP, 0, K_NO_WAIT);

	k_thread_create(&temp_thread_data, temp_stack, STACK_SIZE,
					temp_thread_fn, NULL, NULL, NULL,
					PRIORITY_TEMP, 0, K_NO_WAIT);

	k_thread_create(&light_thread_data, light_stack, STACK_SIZE, 
					light_thread_fn, NULL, NULL, NULL, 
					PRIORITY_LIGHT, 0, K_NO_WAIT);

	k_thread_create(&ble_thread_data, ble_stack, STACK_SIZE,
					ble_thread_fn, NULL, NULL, NULL,
					PRIORITY_BLE, 0, K_NO_WAIT);

	LOG_INF("Exiting");
	return 0;
}



// int main(void)
// {
	

// 	const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(dmic_dev));
// 	int ret;

// 	// Initialise Bluetooth
//     int err;
//     err = bt_enable(NULL);
//     if (err) {
//         printk("Bluetooth init failed: %d\n", err);
//         return 0;
//     }
//     printk("Bluetooth initialized\n");

// 	LOG_INF("DMIC sample");

// 	if (!device_is_ready(dmic_dev)) {
// 		LOG_ERR("%s is not ready", dmic_dev->name);
// 		return 0;
// 	}

// 	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
// 	if (ret < 0) {
// 		return 0;
// 	}

// 	// checks temp sensor readiness
// 	if (!device_is_ready(hts221_dev)) {
// 	LOG_ERR("HTS221 temperature sensor not ready");
// 	return 0;
// 	}

// 	// Manually turn on powr to mic
// 	const struct device *const expander = DEVICE_DT_GET(DT_NODELABEL(sx1509b));
// 	if (!device_is_ready(expander)) {
// 		LOG_ERR("%s is not ready", expander->name);
// 		return 0;
// 	}
// 	gpio_pin_configure(expander, 9, GPIO_OUTPUT_ACTIVE);
// 	gpio_pin_set(expander, 9, 1);

// 	struct pcm_stream_cfg stream = {
// 		.pcm_width = SAMPLE_BIT_WIDTH,
// 		.mem_slab  = &mem_slab,
// 	};
// 	struct dmic_cfg cfg = {
// 		.io = {
// 			/* These fields can be used to limit the PDM clock
// 			 * configurations that the driver is allowed to use
// 			 * to those supported by the microphone.
// 			*/
// 			.min_pdm_clk_freq = 1000000,
// 			.max_pdm_clk_freq = 3250000,
// 			.min_pdm_clk_dc   = 40,
// 			.max_pdm_clk_dc   = 60,
// 		},
// 		.streams = &stream,
// 		.channel = {
// 			.req_num_streams = 1,
// 		},
// 	};

// 	cfg.channel.req_num_chan = 1;
// 	cfg.channel.req_chan_map_lo =
// 		dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
// 	cfg.streams[0].pcm_rate = MAX_SAMPLE_RATE;
// 	cfg.streams[0].block_size =
// 		BLOCK_SIZE(cfg.streams[0].pcm_rate, cfg.channel.req_num_chan);

// 	while (1) {
		
		

// 		struct values data = {0}; // Reset sensor values

// 		// Read temperature and light into struct
// 		read_temperature(&data);
// 		// Detect claps and update struct
// 		ret = do_pdm_transfer(dmic_dev, &cfg, BLOCK_COUNT, &data);
// 		if (ret < 0) {
// 			return 0;
// 		}

// 		// Send over BLE
// 		advertise_thingy(data);

// 		k_sleep(K_SECONDS(5));
// 	}

// 	LOG_INF("Exiting");
// 	return 0;
// }
