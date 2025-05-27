/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Sample app to demonstrate PWM-based servomotor control
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/gpio.h>

static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(DT_NODELABEL(servo));
static const uint32_t min_pulse = DT_PROP(DT_NODELABEL(servo), min_pulse);
static const uint32_t max_pulse = DT_PROP(DT_NODELABEL(servo), max_pulse);
#define STEP PWM_USEC(100)

uint32_t max = max_pulse;

enum direction {
	DOWN,
	UP,
};

#define ONE 6
#define TWO 7
#define THREE 8
#define FOUR 9
#define FIVE 18
#define ZERO 19
#define GPIO_NODE DT_NODELABEL(gpioc)

static int read_speed_level(const struct device *gpio_dev) {
    if (gpio_pin_get(gpio_dev, ONE)) {
		return 1;
	}
    if (gpio_pin_get(gpio_dev, TWO)) {
        return 2;
    }
    if (gpio_pin_get(gpio_dev, THREE)) {
        return 3;
    }
    if (gpio_pin_get(gpio_dev, FOUR)) return 4;
    if (gpio_pin_get(gpio_dev, FIVE)) return 5;
    return 0; // no button pressed
}

void reset_fan(void) {
	printk("Setting servo to min pulse %u\n", min_pulse);
    pwm_set_pulse_dt(&servo, min_pulse);
    k_sleep(K_SECONDS(1));
	pwm_set_pulse_dt(&servo, 0);
}

int main(void)
{
	uint32_t pulse_width = min_pulse;
	uint32_t current_pulse = min_pulse;
	uint32_t delay_ms = 500;
	enum direction dir = UP;
	int ret;

	printk("Servomotor control\n");

	if (!pwm_is_ready_dt(&servo)) {
		printk("Error: PWM device %s is not ready\n", servo.dev->name);
		return 0;
	}

	const struct device *gpio_dev = DEVICE_DT_GET(GPIO_NODE);
    if (!device_is_ready(gpio_dev)) {
        printk("Error: GPIO device not ready\n");
        return 1;
    }

    // Configure pins
    ret = gpio_pin_configure(gpio_dev, ONE, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

    // Configure pins
    ret = gpio_pin_configure(gpio_dev, TWO, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

	// Configure pins
    ret = gpio_pin_configure(gpio_dev, THREE, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

	// Configure pins
    ret = gpio_pin_configure(gpio_dev, FOUR, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

	// Configure pins
    ret = gpio_pin_configure(gpio_dev, FIVE, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

	// Configure pins
    ret = gpio_pin_configure(gpio_dev, ZERO, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 1;
    }

    printk("Starting servo sweep\n");
	int speed = 0;

    while (1) {
		//ret = pwm_set_pulse_dt(&servo, pulse_width);
		ret = pwm_set_pulse_dt(&servo, current_pulse);
		//ret = pwm_set_dt(&servo, current_pulse, PWM_MSEC(20));
		if (ret < 0) {
			printk("Error %d: failed to set pulse width\n", ret);
			return 0;
		}

		//current_pulse = (current_pulse == min_pulse) ? max_pulse : min_pulse;
		speed = read_speed_level(gpio_dev);
		printk("Speed is %d\n", speed);

		if (speed == 0) {
			current_pulse = min_pulse;
		} if (speed == 1) {
			current_pulse = max_pulse;
		} if (speed == 2) {
			current_pulse = (max_pulse + min_pulse) / 2;
		}

		k_sleep(K_MSEC(1000));
    }
	return 0;
}