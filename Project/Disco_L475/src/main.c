#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

// Define GPIO pins
// This may have to change depending on the board being used
#define TRIG_PIN 4
#define ECHO_PIN 5
#define GPIO_NODE DT_NODELABEL(gpioc)

// Define timeout values
#define ECHO_TIMEOUT_US 25000
#define MAX_RANGE_TIMEOUT_US 40000 // CHANGE THIS VALUE?

static void advertise_ultrasonic(uint16_t distance_cm) {
    // Transmit the value via wifi
}

int main(void)
{
    // Define timing variables
    uint32_t start_time, cycles_spent;
    uint32_t stop_time = 0;
    uint64_t nanoseconds_spent;
    uint32_t distance_cm;
    int ret, timeout_occurred;

    // Set-up device
    const struct device *gpio_dev = DEVICE_DT_GET(GPIO_NODE);
    if (!device_is_ready(gpio_dev)) {
        printk("Error: GPIO device not ready\n");
        return 0;
    }

    // Configure pins
    ret = gpio_pin_configure(gpio_dev, TRIG_PIN, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error configuring trigger pin: %d\n", ret);
        return 0;
    }

    ret = gpio_pin_configure(gpio_dev, ECHO_PIN, GPIO_INPUT);
    if (ret < 0) {
        printk("Error configuring echo pin: %d\n", ret);
        return 0;
    }

    while (1) {
        timeout_occurred = 0;

        // Send 10us trigger pulse
        gpio_pin_set(gpio_dev, TRIG_PIN, 1);
        k_busy_wait(10);
        gpio_pin_set(gpio_dev, TRIG_PIN, 0);

        uint32_t echo_start_time = k_cycle_get_32();

        // Wait allocated time for echo pin to be set high
        while (gpio_pin_get(gpio_dev, ECHO_PIN) == 0) {
            if (k_cyc_to_us_floor32(k_cycle_get_32() - echo_start_time) > ECHO_TIMEOUT_US) {
                timeout_occurred = 1;
                break;
            }
        }
        
        // Echo set high
        if (!timeout_occurred) {
            // Start timing
            start_time = k_cycle_get_32();

            // Wait for echo to go low
            while (gpio_pin_get(gpio_dev, ECHO_PIN) == 1) {
                stop_time = k_cycle_get_32();

                if (k_cyc_to_us_floor32(stop_time - start_time) > MAX_RANGE_TIMEOUT_US) {
                    timeout_occurred = 1;
                    break;
                }
            }

            if (!timeout_occurred) {
                // Find distance
                cycles_spent = stop_time - start_time;
                nanoseconds_spent = k_cyc_to_ns_floor64(cycles_spent);
                distance_cm = nanoseconds_spent / 58000; // CHECK THIS VALUE
                
                printk("Distance: %d cm\n", distance_cm);

                // This will then be sent via Wifi
                advertise_ultrasonic(distance_cm);
            }
        }

        // Wait 
        k_sleep(K_MSEC(100));
    }

    return 0;
}
