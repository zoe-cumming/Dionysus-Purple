#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <lvgl.h>
#include <zephyr/drivers/display.h>

// WiFi settings
#define WIFI_SSID "Zoe (2)"
#define WIFI_PSK "Hello" // CONFIGURE

// HTTP GET settings
#define CONFIG_NET_CONFIG_PEER_IPV_ADDR "172.20.10.8"
#define PEER_PORT 1234

// Event callbacks
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

// Semaphores
static K_SEM_DEFINE(sem_wifi, 0, 1);
static K_SEM_DEFINE(sem_ipv4, 0, 1);


void update_display(int fan_speed, int temperature, bool lights) {
    //Reset the screen
    printk("Printing");
    lv_obj_clean(lv_scr_act());
    char buf[32];

    lv_obj_t *fan_label = lv_label_create(lv_scr_act());
    snprintf(buf, sizeof(buf), "Fan Speed: %d", fan_speed);
    lv_label_set_text(fan_label, buf);
    lv_obj_align(fan_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_color(fan_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(fan_label, &lv_font_montserrat_20, 0);

    lv_obj_t *temp_label = lv_label_create(lv_scr_act());
    snprintf(buf, sizeof(buf), "Temperature: %d C", temperature);
    lv_label_set_text(temp_label, buf);
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_text_color(temp_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_20, 0);

    lv_obj_t *light_label = lv_label_create(lv_scr_act());
    snprintf(buf, sizeof(buf), "Lights: %s", lights ? "ON" : "OFF");
    lv_label_set_text(light_label, buf);
    lv_obj_align(light_label, LV_ALIGN_TOP_MID, 0, 180);
    lv_obj_set_style_text_color(light_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(light_label, &lv_font_montserrat_20, 0);
}


// Called when the WiFi is connected
static void on_wifi_connection_event(struct net_mgmt_event_callback *cb,
                                     uint32_t mgmt_event,
                                     struct net_if *iface)
{
    const struct wifi_status *status = (const struct wifi_status *)cb->info;

    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        if (status->status) {
            printk("Error (%d): Connection request failed\r\n", status->status);
        } else {
            printk("Connected!\r\n");
            k_sem_give(&sem_wifi);
        }
    } else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
        if (status->status) {
            printk("Error (%d): Disconnection request failed\r\n", status->status);
        } else {
            printk("Disconnected\r\n");
            k_sem_take(&sem_wifi, K_NO_WAIT);
        }
    }
}

// Event handler for WiFi management events
static void on_ipv4_obtained(struct net_mgmt_event_callback *cb,
                             uint32_t mgmt_event,
                             struct net_if *iface)
{
    // Signal that the IP address has been obtained
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        k_sem_give(&sem_ipv4);
    }
}

// Initialize the WiFi event callbacks
void wifi_init(void)
{
    // Initialize the event callbacks
    net_mgmt_init_event_callback(&wifi_cb,
                                 on_wifi_connection_event,
                                 NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_init_event_callback(&ipv4_cb,
                                 on_ipv4_obtained,
                                 NET_EVENT_IPV4_ADDR_ADD);
    
    // Add the event callbacks
    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_add_event_callback(&ipv4_cb);
}

// Connect to the WiFi network (blocking)
int wifi_connect(char *ssid, char *psk)
{
    int ret;
    struct net_if *iface;
    struct wifi_connect_req_params params;

    // Get the default networking interface
    iface = net_if_get_default();

    // Fill in the connection request parameters
    params.ssid = (const uint8_t *)ssid;
    params.ssid_length = strlen(ssid);
    params.psk = (const uint8_t *)psk;
    params.psk_length = strlen(psk);
    params.security = WIFI_SECURITY_TYPE_PSK;
    params.band = WIFI_FREQ_BAND_UNKNOWN;
    params.channel = WIFI_CHANNEL_ANY;
    params.mfp = WIFI_MFP_OPTIONAL;

    // Connect to the WiFi network
    ret = net_mgmt(NET_REQUEST_WIFI_CONNECT,
                   iface,
                   &params,
                   sizeof(params));

    // Wait for the connection to complete
    k_sem_take(&sem_wifi, K_FOREVER);

    return ret;
}

// Wait for IP address (blocking)
void wifi_wait_for_ip_addr(void)
{
    struct wifi_iface_status status;
    struct net_if *iface;
    char ip_addr[NET_IPV4_ADDR_LEN];
    char gw_addr[NET_IPV4_ADDR_LEN];

    // Get interface
    iface = net_if_get_default();

    // Wait for the IPv4 address to be obtained
    k_sem_take(&sem_ipv4, K_FOREVER);

    // Get the WiFi status
    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS,
                 iface,
                 &status,
                 sizeof(struct wifi_iface_status))) {
        printk("Error: WiFi status request failed\r\n");
    }

    // Get the IP address
    memset(ip_addr, 0, sizeof(ip_addr));
    if (net_addr_ntop(AF_INET,
                      &iface->config.ip.ipv4->unicast[0].ipv4.address.in_addr,
                      ip_addr,
                      sizeof(ip_addr)) == NULL) {
        printk("Error: Could not convert IP address to string\r\n");
    }

    // Get the gateway address
    memset(gw_addr, 0, sizeof(gw_addr));
    if (net_addr_ntop(AF_INET,
                      &iface->config.ip.ipv4->gw,
                      gw_addr,
                      sizeof(gw_addr)) == NULL) {
        printk("Error: Could not convert gateway address to string\r\n");
    }

    // Print the WiFi status
    printk("WiFi status:\r\n");
    if (status.state >= WIFI_STATE_ASSOCIATED) {
        printk("  SSID: %-32s\r\n", status.ssid);
        printk("  Band: %s\r\n", wifi_band_txt(status.band));
        printk("  Channel: %d\r\n", status.channel);
        printk("  Security: %s\r\n", wifi_security_txt(status.security));
        printk("  IP address: %s\r\n", ip_addr);
        printk("  Gateway: %s\r\n", gw_addr);
    }
}

// Disconnect from the WiFi network
int wifi_disconnect(void)
{
    int ret;
    struct net_if *iface = net_if_get_default();

    ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);

    return ret;
}

int main(void)
{
	int ret, sock;
    char setting[16];
    int value = 1;

    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        printk("Display not ready\n");
        return 0;
    }

    lv_init();
    display_blanking_off(display);
    lv_task_handler();
    update_display(100, 20, true);

    while (1) {
        lv_timer_handler();    // Refresh UI
        k_msleep(5);           // Run every ~5ms
    }

    printk("WiFi Connection and TCP Client\r\n");

    // Initialize WiFi
    wifi_init();

    // Connect to the WiFi network (blocking)
    ret = wifi_connect(WIFI_SSID, WIFI_PSK);
    if (ret < 0) {
        printk("Error (%d): WiFi connection failed\r\n", ret);
        return 0;
    }

    // Wait to receive an IP address (blocking)
    wifi_wait_for_ip_addr();

    sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		printk("socket: %d", -errno);
		return 0;
	}

    struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(PEER_PORT),
	};

    net_addr_pton(AF_INET, CONFIG_NET_CONFIG_PEER_IPV_ADDR, &addr.sin_addr);

	if (zsock_connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		printk("connect: %d", -errno);
		return 0;
	}

    printk("Connected! Sending...\n");

	while (1) {
        snprintf(setting, sizeof(setting) - 1, "%d\n", value);
        ret = zsock_send(sock, setting, sizeof(setting) - 1, 0);
        if (ret < 0) {
            printk("send: %d", -errno);
		    return 0;
        }

        value += 1;
        if (value > 5) {
            value = 1;
        }

        k_msleep(1000);
	}

    // Close the socket
    printk("Closing socket\r\n");
    zsock_close(sock);

    return 0;
}