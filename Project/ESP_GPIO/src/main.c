#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define PIN_ZERO 6
#define PIN_ONE 7
#define PIN_TWO 8
#define GPIO_NODE DT_NODELABEL(gpio0)

int main(void)
{
    int ret;
	const struct device *gpio_dev = DEVICE_DT_GET(GPIO_NODE);
    if (!device_is_ready(gpio_dev)) {
        printk("Error: GPIO device not ready\n");
        return 1;
    }

    // Configure pins
    ret = gpio_pin_configure(gpio_dev, PIN_ZERO, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

    // Configure pins
    ret = gpio_pin_configure(gpio_dev, PIN_ONE, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

	// Configure pins
    ret = gpio_pin_configure(gpio_dev, PIN_TWO, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

    gpio_pin_set(gpio_dev, PIN_ZERO, 0);
    gpio_pin_set(gpio_dev, PIN_ONE, 0);
    gpio_pin_set(gpio_dev, PIN_TWO, 0);
    k_msleep(2000);

	while (1) {
		gpio_pin_set(gpio_dev, PIN_ZERO, 1);
        gpio_pin_set(gpio_dev, PIN_ONE, 0);
        gpio_pin_set(gpio_dev, PIN_TWO, 0);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, PIN_ZERO, 0);
		gpio_pin_set(gpio_dev, PIN_ONE, 1);
        gpio_pin_set(gpio_dev, PIN_TWO, 0);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, PIN_ZERO, 1);
		gpio_pin_set(gpio_dev, PIN_ONE, 1);
        gpio_pin_set(gpio_dev, PIN_TWO, 0);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, PIN_ZERO, 0);
		gpio_pin_set(gpio_dev, PIN_ONE, 0);
        gpio_pin_set(gpio_dev, PIN_TWO, 1);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, PIN_ZERO, 1);
		gpio_pin_set(gpio_dev, PIN_ONE, 0);
        gpio_pin_set(gpio_dev, PIN_TWO, 1);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, PIN_ZERO, 1);
		gpio_pin_set(gpio_dev, PIN_ONE, 1);
        gpio_pin_set(gpio_dev, PIN_TWO, 1);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, PIN_ZERO, 0);
		gpio_pin_set(gpio_dev, PIN_ONE, 0);
        gpio_pin_set(gpio_dev, PIN_TWO, 0);
	}

	return 1;
}