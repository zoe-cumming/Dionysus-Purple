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

#define PWM_PERIOD PWM_MSEC(20)

enum direction {
    LEFT,
    RIGHT
};

#define PIN_ZERO 3
#define PIN_ONE 4
#define PIN_TWO 5
#define GPIO_NODE DT_NODELABEL(gpioc)

static int read_speed_level(const struct device *gpio_dev) {
    if (!gpio_pin_get(gpio_dev, PIN_ZERO) && !gpio_pin_get(gpio_dev, PIN_ONE) && !gpio_pin_get(gpio_dev, PIN_TWO)) {
        printk("Speed 0\n");
		return 0;
	}
    if (gpio_pin_get(gpio_dev, PIN_ZERO) && !gpio_pin_get(gpio_dev, PIN_ONE) && !gpio_pin_get(gpio_dev, PIN_TWO)) {
        printk("Speed 1\n");
        return 1;
    }
    if (!gpio_pin_get(gpio_dev, PIN_ZERO) && gpio_pin_get(gpio_dev, PIN_ONE) && !gpio_pin_get(gpio_dev, PIN_TWO)) {
        printk("Speed 2\n");
        return 2;
    }
    if (gpio_pin_get(gpio_dev, PIN_ZERO) && gpio_pin_get(gpio_dev, PIN_ONE) && !gpio_pin_get(gpio_dev, PIN_TWO)) {
        printk("Speed 3\n");
        return 3;
    }
    if (!gpio_pin_get(gpio_dev, PIN_ZERO) && !gpio_pin_get(gpio_dev, PIN_ONE) && gpio_pin_get(gpio_dev, PIN_TWO)) {
        printk("Speed 4\n");
        return 4;
    }
    if (gpio_pin_get(gpio_dev, PIN_ZERO) && !gpio_pin_get(gpio_dev, PIN_ONE) && gpio_pin_get(gpio_dev, PIN_TWO)) {
        printk("Speed 5\n");
        return 5;
    }
    if (gpio_pin_get(gpio_dev, PIN_ZERO) && gpio_pin_get(gpio_dev, PIN_ONE) && gpio_pin_get(gpio_dev, PIN_TWO)) {
        printk("None\n");
        return 0;
    }
    printk("Ruh Roh\n");
    return -1; // no button pressed
}

int main(void)
{
    if (!pwm_is_ready_dt(&servo)) {
        printk("Error: PWM device %s is not ready\n", servo.dev->name);
        return 0;
    }

    const struct device *gpio_dev = DEVICE_DT_GET(GPIO_NODE);
    if (!device_is_ready(gpio_dev)) {
        printk("Error: GPIO device not ready\n");
        return 1;
    }

    gpio_pin_configure(gpio_dev, PIN_ZERO, GPIO_INPUT);
    gpio_pin_configure(gpio_dev, PIN_ONE, GPIO_INPUT);
    gpio_pin_configure(gpio_dev, PIN_TWO, GPIO_INPUT);

    uint32_t pulse = min_pulse;
    int speed = 0;
    int ret;
    printk("Servo continuous control started\n");

    while (1) {
        speed = read_speed_level(gpio_dev);

        if (speed == 0) {
            k_sleep(K_MSEC(200));
            continue;
        }

        uint32_t step_size = (max_pulse - min_pulse) / (20 - (speed * 3));
        pulse += step_size;
        if (pulse > max_pulse) {
            pulse = min_pulse;
        }

        ret = pwm_set_dt(&servo, PWM_PERIOD, pulse);
        if (ret < 0) {
            printk("PWM error: %d\n", ret);
        } else {
            printk("Speed %d -> pulse %u\n", speed, pulse);
        }

        k_sleep(K_MSEC(50));
    }

    return 0;
}