#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define ONE 6
#define TWO 7
#define THREE 8
#define FOUR 9
#define FIVE 18
#define ZERO 19
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
    ret = gpio_pin_configure(gpio_dev, ONE, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

    // Configure pins
    ret = gpio_pin_configure(gpio_dev, TWO, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

	// Configure pins
    ret = gpio_pin_configure(gpio_dev, THREE, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

	// Configure pins
    ret = gpio_pin_configure(gpio_dev, FOUR, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

	// Configure pins
    ret = gpio_pin_configure(gpio_dev, FIVE, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

	// Configure pins
    ret = gpio_pin_configure(gpio_dev, ZERO, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

    gpio_pin_set(gpio_dev, ONE, 1);

	while (1) {
		gpio_pin_set(gpio_dev, ONE, 1);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, ONE, 0);
		gpio_pin_set(gpio_dev, TWO, 1);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, TWO, 0);
		gpio_pin_set(gpio_dev, THREE, 1);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, THREE, 0);
		gpio_pin_set(gpio_dev, FOUR, 1);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, FOUR, 0);
		gpio_pin_set(gpio_dev, FIVE, 1);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, FIVE, 0);
		gpio_pin_set(gpio_dev, ZERO, 1);
		k_sleep(K_SECONDS(2));
		gpio_pin_set(gpio_dev, ZERO, 0);
	}

	return 1;
}