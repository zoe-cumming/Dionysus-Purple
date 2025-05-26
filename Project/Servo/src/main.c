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

static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(DT_NODELABEL(servo));
static const uint32_t min_pulse = DT_PROP(DT_NODELABEL(servo), min_pulse);
static const uint32_t max_pulse = DT_PROP(DT_NODELABEL(servo), max_pulse);


#define STEP PWM_USEC(100)

enum direction {
	DOWN,
	UP,
};

int main(void)
{
	uint32_t pulse_width = min_pulse;
	uint32_t current_pulse = min_pulse;
	enum direction dir = UP;
	int ret;

	printk("Servomotor control\n");
	printk("Pulse is: %u", min_pulse);
	printk("Pulse is: %u", max_pulse);

	printk("PWM dev: %s, channel: %d\n", servo.dev->name, servo.channel);
	printk("min: %d, max: %d\n", min_pulse, max_pulse);

	if (!pwm_is_ready_dt(&servo)) {
		printk("Error: PWM device %s is not ready\n", servo.dev->name);
		return 0;
	}

	if (!device_is_ready(servo.dev)) {
		printk("PWM device not ready!\n");
		return 0;
	}

	while (1) {
		ret = pwm_set_pulse_dt(&servo, pulse_width);
		//ret = pwm_set_pulse_dt(&servo, current_pulse);
		//ret = pwm_set_dt(&servo, current_pulse, PWM_MSEC(20));
		if (ret < 0) {
			printk("Error %d: failed to set pulse width\n", ret);
			return 0;
		}

		//current_pulse = (current_pulse == min_pulse) ? max_pulse : min_pulse;
		printk("Trying...");

		if (dir == DOWN) {
			if (pulse_width <= min_pulse) {
				dir = UP;
				pulse_width = min_pulse;
			} else {
				pulse_width -= STEP;
			}
		} else {
			pulse_width += STEP;

			if (pulse_width >= max_pulse) {
				dir = DOWN;
				pulse_width = max_pulse;
			}
		}

		k_sleep(K_MSEC(500));
	}
	return 0;
}