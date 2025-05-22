#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);
#define WIFI_SSID       "Get Your Own WIFI" //"Super6"
#define WIFI_PASS       "j1ms!aw3M" //"L10n5Br0nc05?"
#define SERVER_IP       "172.17.11.168"  // Your laptop IP on hotspot
#define SERVER_PORT     12345

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

static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ip_cb;

static int sock = -1;

static void connect_to_server(void)
{
    struct sockaddr_in addr;
    int ret;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("Failed to create socket: %d", sock);
        return;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    LOG_INF("Connecting to server %s:%d...", SERVER_IP, SERVER_PORT);

    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERR("Socket connect failed: %d", ret);
        close(sock);
        sock = -1;
        return;
    }

    LOG_INF("Connected to server!");
    char *msg = "Hello from M5Core!\n";
    send(sock, msg, strlen(msg), 0);
    LOG_INF("Message sent");

    char buffer[64];
    int len = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (len > 0) {
        buffer[len] = '\0';
        LOG_INF("Received from server: %s", buffer);
    }
    close(sock);
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                               uint32_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        struct wifi_status *status = (struct wifi_status *)cb->info;

        if (status->status) {
            LOG_ERR("Wi-Fi connection failed (%d)", status->status);
        } else {
            LOG_INF("Wi-Fi connected");
            connect_to_server();
        }
    } else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        LOG_INF("Wi-Fi disconnected");
        if (sock >= 0) {
            close(sock);
            sock = -1;
        }
    }
}

static void ip_event_handler(struct net_mgmt_event_callback *cb,
                             uint32_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {

        // /* Now we can connect to the server */
        // struct net_if *iface = net_if_get_default();
        // char ip_str[NET_IPV4_ADDR_LEN];

        // if (iface == NULL) {
        //     LOG_INF("No default network interface");
        //     return;
        // }

        // if (iface->config.ip.ipv4 == NULL) {
        //     LOG_INF("No IPv4 config found");
        //     return;
        // }

        // struct net_if_ipv4 *ipv4 = iface->config.ip.ipv4;
        // if (ipv4->unicast_count == 0) {
        //     LOG_INF("No unicast IPv4 addresses");
        //     return;
        // }

        // // Print first unicast address
        // net_addr_ntop(AF_INET, &ipv4->unicast[0].ipv4.address.in_addr, ip_str, sizeof(ip_str));
        // LOG_INF("Board IP address: %s", ip_str);
        //connect_to_server();
    }
}

int main(void)
{
    // Define timing variables
    uint32_t start_time, cycles_spent;
    uint32_t stop_time = 0;
    uint64_t nanoseconds_spent;
    uint32_t distance_cm;
    int ret, timeout_occurred;

    struct net_if *iface = net_if_get_default();

    /* Init and add callbacks */
    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);

    net_mgmt_init_event_callback(&ip_cb, ip_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&ip_cb);

    struct wifi_connect_req_params params = {
        .ssid = WIFI_SSID,
        .ssid_length = strlen(WIFI_SSID),
        .psk = WIFI_PASS,
        .psk_length = strlen(WIFI_PASS),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
    };

    LOG_INF("Connecting to Wi-Fi %s...", WIFI_SSID);
    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret) {
        LOG_ERR("Wi-Fi connect request failed: %d", ret);
    }

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
