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
#include <zephyr/net/mqtt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_if.h>

// WiFi settings
#define WIFI_SSID "Super6"
#define WIFI_PSK "L10n5Br0nc05?" // CONFIGURE
#define HIVEMQ_HOSTNAME "broker.emqx.io"
#define BROKER_IP "44.232.241.40"
#define HIVEMQ_PORT 1883
#define CLIENT_ID "m5stack-zephyr-client"
#define MQTT_SUBSCRIBE_TOPIC "python/mqtt"
#define HIVEMQ_USERNAME "emqx"
#define HIVEMQ_PASSWORD "public"

// Buffer sizes
#define MQTT_CLIENT_RX_BUF_LEN 128
#define MQTT_CLIENT_TX_BUF_LEN 128

static struct mqtt_client client;
static struct sockaddr_storage broker;
static struct pollfd fds[1];
static int nfds;

static uint8_t rx_buff[MQTT_CLIENT_RX_BUF_LEN];
static uint8_t tx_buff[MQTT_CLIENT_TX_BUF_LEN];

// HTTP GET settings
#define CONFIG_NET_CONFIG_PEER_IPV_ADDR "192.168.68.60"
#define PEER_PORT 1234

// Event callbacks
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

// Semaphores
static K_SEM_DEFINE(sem_mqtt_connected, 0, 1);
static K_SEM_DEFINE(dns_sem, 0, 1);
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

void dns_result_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data) {
	char hr_addr[NET_IPV4_ADDR_LEN];
	char *hr_family;
	void *addr;
    printf("Resolving ...\n");
	switch (status) {
	case DNS_EAI_CANCELED:
		printf("DNS query was canceled\n");
		return;
	case DNS_EAI_FAIL:
		printf("DNS resolve failed\n");
		return;
	case DNS_EAI_NODATA:
		printf("Cannot resolve address\n");
		return;
	case DNS_EAI_ALLDONE:
		printf("DNS resolving finished\n");
        k_sem_give(&dns_sem);
		return;
	case DNS_EAI_INPROGRESS:
		break;
	default:
		printf("DNS resolving error (%d)\n", status);
		return;
	}

	if (!info) {
		return;
	}

	if (info->ai_family == AF_INET) {
		hr_family = "IPv4";
		addr = &net_sin(&info->ai_addr)->sin_addr;
        struct sockaddr_in *broker_addr = (struct sockaddr_in *)&broker;
        broker_addr->sin_family = AF_INET;
        broker_addr->sin_port = htons(HIVEMQ_PORT);
        memcpy(&broker_addr->sin_addr, &net_sin(&info->ai_addr)->sin_addr, sizeof(struct in_addr));
        net_addr_ntop(AF_INET, &broker_addr->sin_addr, hr_addr, sizeof(hr_addr));
        printf("Resolved %s to IPv4 address: %s\n", HIVEMQ_HOSTNAME, hr_addr);
	} else if (info->ai_family == AF_INET6) {
		hr_family = "IPv6";
		addr = &net_sin6(&info->ai_addr)->sin6_addr;
	} else {
		printf("Invalid IP address family %d\n", info->ai_family);
		return;
	}

	printf("%s %s address: %s\n", user_data ? (char *)user_data : "<null>",
		hr_family, net_addr_ntop(info->ai_family, addr, hr_addr, sizeof(hr_addr)));
}

static int resolve_dns(void)
{
    struct dns_resolve_context *dns_ctx = dns_resolve_get_default();
    if (!dns_ctx) {
        printk("No DNS context available\n");
        return -1;
    }

    printk("Resolving hostname: %s\n", HIVEMQ_HOSTNAME);
    int ret = dns_get_addr_info(HIVEMQ_HOSTNAME, DNS_QUERY_TYPE_A, NULL, dns_result_cb, (void*)HIVEMQ_HOSTNAME, 10000);
    if (ret) {
        printk("Failed to start DNS query: %d\n", ret);
        return ret;
    }

    ret = k_sem_take(&dns_sem, K_SECONDS(10));
    if (ret) {
        printk("DNS resolve timeout\n");
        return -2;
    }
    return 0;
}

static void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result == 0) {
            printk("MQTT client connected\n");
            k_sem_give(&sem_mqtt_connected);
        } else {
            printk("MQTT connect failed: %d\n", evt->result);
        }
        break;
    case MQTT_EVT_DISCONNECT:
        printk("MQTT client disconnected\n");
        break;
    case MQTT_EVT_PUBLISH:
        {
            const struct mqtt_publish_param *p = &evt->param.publish;
            uint16_t len = p->message.payload.len;
            if (len > sizeof(rx_buff) - 1) {
                len = sizeof(rx_buff) - 1;
            }
            int ret = mqtt_read_publish_payload(client, rx_buff, len);
            if (ret < 0) {
                printk("Failed to read MQTT payload: %d\n", ret);
                break;
            }
            rx_buff[len] = '\0';
            printk("MQTT message received on topic %.*s: %s\n",
                   p->message.topic.topic.size,
                   p->message.topic.topic.utf8,
                   rx_buff);
        }
        break;
    default:
        printk("MQTT event: %d\n", evt->type);
        break;
    }
}

static int mqtt_connect_client(void)
{
    int ret = resolve_dns();
    if (ret) {
        return ret;
    }
    
    mqtt_client_init(&client);

    client.broker = &broker;
    client.evt_cb = mqtt_evt_handler;

    client.client_id.utf8 = (uint8_t *)CLIENT_ID;
    client.client_id.size = strlen(CLIENT_ID);

    client.protocol_version = MQTT_VERSION_3_1_1;

    struct mqtt_sec_config *tls_config = NULL; // No TLS here, plain MQTT on port 1883

    client.transport.type = MQTT_TRANSPORT_NON_SECURE;
    client.rx_buf = rx_buff;
    client.rx_buf_size = sizeof(rx_buff);
    client.tx_buf = tx_buff;
    client.tx_buf_size = sizeof(tx_buff);

    
    struct mqtt_utf8 username = {
        .utf8 = (uint8_t *)HIVEMQ_USERNAME,
        .size = strlen(HIVEMQ_USERNAME)
    };
    struct mqtt_utf8 password = {
        .utf8 = (uint8_t *)HIVEMQ_PASSWORD,
        .size = strlen(HIVEMQ_PASSWORD)
    };

    client.password = &password;
	client.user_name = &username;


    ret = mqtt_connect(&client);
    if (ret) {
        printk("MQTT connect failed: %d\n", ret);
        return ret;
    }

    return 0;
}

static int mqtt_subscribe_topic(const char *topic)
{
    struct mqtt_topic topics[1];
    struct mqtt_subscription_list subscription;
    topics[0].topic.utf8 = (uint8_t *)topic;
    topics[0].topic.size = strlen(topic);
    topics[0].qos = MQTT_QOS_1_AT_LEAST_ONCE;

    subscription.list = topics;
    subscription.list_count = 1;
    subscription.message_id = 1;
    printf("Subscribing to topic: %s\n", topic);
    int ret = mqtt_subscribe(&client, &subscription);

    if (ret) {
        printk("Failed to subscribe to topic %s: %d\n", topic, ret);
        return ret;
    }

    printk("Subscribed to topic %s\n", topic);
    return 0;
}

static int mqtt_publish_message(const char *topic, const char *message)
{
    struct mqtt_publish_param param;

    param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = (uint8_t *)message;
    param.message.payload.len = strlen(message);
    param.message_id = 1;
    param.dup_flag = 0;
    param.retain_flag = 0;

    int ret = mqtt_publish(&client, &param);
    if (ret) {
        printk("Failed to publish message: %d\n", ret);
        return ret;
    }

    printk("Published message: %s to topic: %s\n", message, topic);
    return 0;
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
	int ret;// sock;
    //char setting[16];
    //int value = 1;

    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        printk("Display not ready\n");
        return 0;
    }

    lv_init();
    display_blanking_off(display);
    lv_task_handler();
    update_display(100, 20, true);

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

    // Resolve MQTT broker IP address
    printk("Starting simple MQTT client\r\n");

    ret = mqtt_connect_client();
    if (ret < 0) {
        printk("Failed to initialize and connect to MQTT broker, error: %d\n", ret);
        return 1;
    }
    
    if (client.transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds[0].fd = client.transport.tcp.sock;
	}
    fds[0].events = ZSOCK_POLLIN;
    nfds = 1;

    // Poll for MQTT events until connected
    while (k_sem_count_get(&sem_mqtt_connected) == 0) {
        mqtt_input(&client);
        mqtt_live(&client);
        k_sleep(K_MSEC(100)); // Give some delay for CPU
        printk("waiting");
    }

    if (client.internal.state) {
        ret = mqtt_subscribe_topic(MQTT_SUBSCRIBE_TOPIC);
        if (ret < 0) {
            printf("Failed to subscribe to topic, error: %d\n", ret);
            return 1;
        }
    } else {
        printf("MQTT client not connected, cannot subscribe.\n");
    }

    while (1) {
        /* Handle incoming MQTT events - received messages, etc. */
        if (poll(fds, nfds, 10000) < 0) {
            printf("Error in poll: %d\n", errno);
            break;
        }

        if (fds[0].revents & POLLIN) {
            ret = mqtt_input(&client);
            if (ret != 0) {
                printf("Error in mqtt_input: %d\n", ret);
                break;
            }
        }

        if (fds[0].revents & POLLERR) {
            printk("POLLERR\n");
            break;
        }

        if (fds[0].revents & POLLHUP) {
            printk("POLLHUP\n");
            break;
        }    

        lv_task_handler();
        k_msleep(1000);
    }

    mqtt_disconnect(&client);
    /*sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
    zsock_close(sock);*/

    return 0;
}