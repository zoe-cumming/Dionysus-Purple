/*
Clap detection using Thingy52 PDM mic
*/

#include <stdlib.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(dmic_sample);

#define MAX_SAMPLE_RATE  16000
#define SAMPLE_BIT_WIDTH 16
#define BYTES_PER_SAMPLE sizeof(int16_t)
/* Milliseconds to wait for a block to be read. */
#define READ_TIMEOUT     1000

// alias'
#define HTS221_NODE DT_ALIAS(temphum)
const struct device *hts221_dev = DEVICE_DT_GET(HTS221_NODE);
#define APDS9960_NODE DT_ALIAS(light)
const struct device *light_sensor = DEVICE_DT_GET(APDS9960_NODE);


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
				LOG_INF("Clap at sample %d", i);
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

void read_light(const struct device *light_sensor, struct values *data)
{
    struct sensor_value lux;

    if (!device_is_ready(light_sensor)) {
        LOG_ERR("Light sensor not ready");
        return;
    }

    if (sensor_sample_fetch(light_sensor) == 0 &&
        sensor_channel_get(light_sensor, SENSOR_CHAN_LIGHT, &lux) == 0) {

        data->light = (uint16_t)lux.val1;  // Store integer lux value
        data->newlight = 1;
        LOG_INF("Ambient light: %d lux", lux.val1);
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

	// Pack values: top byte = temp, bottom byte = light
    uint16_t major_part = ((data.temp & 0xFF) << 8) | (data.light & 0xFF);
    // Clap into upper byte of minor, lower byte can be a flag (0x01) if needed
    uint16_t minor_part = ((data.clap & 0xFF) << 8) | 0x01;

    // Set the measurement into MAJOR or MINOR
	// if (data.newtemp && data.newclap) {
    // major_part = data.temp;   // temperature
    // minor_part = data.clap;   // clap count
	// } else if (data.newtemp) {
	// 	major_part = 0;
	// 	minor_part = data.temp;
	// } else if (data.newlight) {
	// 	major_part = 1;
	// 	minor_part = data.light;
	// } else if (data.newclap) {
	// 	major_part = 2;
	// 	minor_part = data.clap;
	// }

    sys_put_be16(major_part, &beacon_data[MAJOR_OFFSET]);
    sys_put_be16(minor_part, &beacon_data[MINOR_OFFSET]);

    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, beacon_data, sizeof(beacon_data)),
    };

    bt_le_adv_stop();
    int err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("[ADVERTISING] Measurement\n");

    k_sleep(K_MSEC(200));
    bt_le_adv_stop();
}

int main(void)
{
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

	// Manually turn on powr to mic
	const struct device *const expander = DEVICE_DT_GET(DT_NODELABEL(sx1509b));
	if (!device_is_ready(expander)) {
		LOG_ERR("%s is not ready", expander->name);
		return 0;
	}
	gpio_pin_configure(expander, 9, GPIO_OUTPUT_ACTIVE);
	gpio_pin_set(expander, 9, 1);

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
		struct values data = {0}; // Reset sensor values

		// Read temperature and light into struct
		read_temperature(&data);
		read_light(light_sensor, &data);
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