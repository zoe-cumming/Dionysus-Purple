#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/net/net_ip.h>

// WiFi settings
#define WIFI_SSID "Get Your Own WIFI"
#define WIFI_PSK "j1ms!aw3M"

// HTTP GET settings
#define SERVER_IP "192.168.1.100"
#define SERVER_PORT 1234

// Event callbacks
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;

// Semaphores
static K_SEM_DEFINE(sem_wifi, 0, 1);
static K_SEM_DEFINE(sem_ipv4, 0, 1);

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
    socklen_t optlen = sizeof(int);
	int ret, sock, client_sock, opt;
    struct sockaddr_in6 client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char setting[16];

    struct sockaddr_in6 addr = {
		.sin6_family = AF_INET6,
		.sin6_addr = IN6ADDR_ANY_INIT,
		.sin6_port = htons(SERVER_PORT),
	};

    printk("WiFi Connection and TCP Server\r\n");

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

	ret = zsock_getsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, &optlen);
	if (ret == 0 && opt) {
		printk("IPV6_V6ONLY option is on, turning it off.");

		opt = 0;
		ret = zsock_getsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, &optlen);
		if (ret < 0) {
			printk("Cannot turn off IPV6_V6ONLY option");
		} else {
			printk("Sharing same socket between IPv6 and IPv4");
		}
	}

	if (zsock_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		printk("bind: %d", -errno);
		return 0;
	}

    if (zsock_listen(sock, 5) < 0) {
        printk("listen: %d", -errno);
        return 0;
    }

    while(1) {
        // Accept an incoming connection
        client_sock = zsock_accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            printk("Error (%d): Could not accept connection\r\n", -errno);
            continue;
        }
        printk("Connection accepted from a client\r\n");

        // Receive data from the connected client
        ssize_t bytes_received;
        while ((bytes_received = zsock_recv(client_sock, setting, sizeof(setting) - 1, 0)) > 0) {
            setting[bytes_received] = '\0';
            printk("Received: %s\r\n", setting);
        }

        if (bytes_received < 0) {
            printk("Error (%d): Receive failed or connection closed by client\r\n", -errno);
        }

        // Close the client socket
        printk("Closing client connection\r\n");
        zsock_close(client_sock);
    }

    // Close the socket
    printk("Closing socket\r\n");
    zsock_close(sock);

    return 0;
}