/*
 * Copyright (c) 2019 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/net/net_ip.h>

#define VIDEO_DEV_SW "VIDEO_SW_GENERATOR"
#define MY_PORT 5000
#define MAX_CLIENT_QUEUE 1

// WiFi settings
#define WIFI_SSID "//"
#define WIFI_PSK "//"

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
    k_sem_take(&sem_wifi,K_FOREVER);

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


static ssize_t sendall(int sock, const void *buf, size_t len)
{
	while (len) {
		ssize_t out_len = zsock_send(sock, buf, len, 0);

		if (out_len < 0) {
			return out_len;
		}
		buf = (const char *)buf + out_len;
		len -= out_len;
	}

	return 0;
}

int main(void)
{
	static struct sockaddr_in addr, client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	struct video_buffer *buffers[CONFIG_VIDEO_BUFFER_POOL_NUM_MAX];
	struct video_buffer *vbuf = &(struct video_buffer){};
	static int i, ret, sock, client;
	static struct video_format fmt;
	static struct video_caps caps;
	struct video_frmival frmival;
	struct video_frmival_enum fie;
	enum video_buf_type type = VIDEO_BUF_TYPE_OUTPUT;
	size_t bsize;

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

	const struct device *const video = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));

	if (!device_is_ready(video)) {
		printk("%s: video device not ready.", video->name);
		return 0;
	}

	// Prepare network
	(void)memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MY_PORT);

	sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		printk("Failed to create TCP socket: %d", errno);
		return 0;
	}

	ret = zsock_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		printk("Failed to bind TCP socket: %d", errno);
		zsock_close(sock);
		return 0;
	}

	ret = zsock_listen(sock, MAX_CLIENT_QUEUE);
	if (ret < 0) {
		printk("Failed to listen on TCP socket: %d", errno);
		zsock_close(sock);
		return 0;
	}

	/* Get capabilities */
	caps.type = type;
	if (video_get_caps(video, &caps)) {
		printk("Unable to retrieve video capabilities");
		return 0;
	}

	printk("- Capabilities:");
	while (caps.format_caps[i].pixelformat) {
		const struct video_format_cap *fcap = &caps.format_caps[i];
		/* fourcc to string */
		printk("  %s width [%u; %u; %u] height [%u; %u; %u]",
			VIDEO_FOURCC_TO_STR(fcap->pixelformat),
			fcap->width_min, fcap->width_max, fcap->width_step,
			fcap->height_min, fcap->height_max, fcap->height_step);
		i++;
	}

	/* Get default/native format */
	fmt.type = type;
	if (video_get_format(video, &fmt)) {
		printk("Unable to retrieve video format");
		return 0;
	}

	fmt.height = CONFIG_VIDEO_FRAME_HEIGHT;
	fmt.width = CONFIG_VIDEO_FRAME_WIDTH;

	if (strcmp(CONFIG_VIDEO_PIXEL_FORMAT, "")) {
		fmt.pixelformat = VIDEO_FOURCC_FROM_STR(CONFIG_VIDEO_PIXEL_FORMAT);
	}

	printk("- Video format: %s %ux%u",
		VIDEO_FOURCC_TO_STR(fmt.pixelformat), fmt.width, fmt.height);

	if (video_set_format(video, &fmt)) {
		printk("Unable to set format");
		return 0;
	}

	if (!video_get_frmival(video, &frmival)) {
		printk("- Default frame rate : %f fps",
			1.0 * frmival.denominator / frmival.numerator);
	}

	printk("- Supported frame intervals for the default format:");
	memset(&fie, 0, sizeof(fie));
	fie.format = &fmt;
	while (video_enum_frmival(video, &fie) == 0) {
		if (fie.type == VIDEO_FRMIVAL_TYPE_DISCRETE) {
			printk("   %u/%u", fie.discrete.numerator, fie.discrete.denominator);
		} else {
			printk("   [min = %u/%u; max = %u/%u; step = %u/%u]",
				fie.stepwise.min.numerator, fie.stepwise.min.denominator,
				fie.stepwise.max.numerator, fie.stepwise.max.denominator,
				fie.stepwise.step.numerator, fie.stepwise.step.denominator);
		}
		fie.index++;
	}

	/* Size to allocate for each buffer */
	if (caps.min_line_count == LINE_COUNT_HEIGHT) {
		bsize = fmt.pitch * fmt.height;
	} else {
		bsize = fmt.pitch * caps.min_line_count;
	}

	// Allocate video buffers
	for (i = 0; i < ARRAY_SIZE(buffers); i++) {
		buffers[i] = video_buffer_aligned_alloc(bsize, CONFIG_VIDEO_BUFFER_POOL_ALIGN,
							K_FOREVER);
		if (buffers[i] == NULL) {
			printk("Unable to alloc video buffer");
			return 0;
		}
		buffers[i]->type = type;
	}

	// Allow connections
	while (1) {
		printk("TCP: Waiting for client...\n");

		client = zsock_accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
		if (client < 0) {
			printk("Failed to accept: %d\n", errno);
			return 0;
		}

		printk("TCP: Accepted connection\n");

		// Enqueue buffers
		for (i = 0; i < ARRAY_SIZE(buffers); i++) {
			video_enqueue(video, buffers[i]);
		}

		// Capture video
		if (video_stream_start(video, type)) {
			printk("Unable to start video");
			return 0;
		}

		printk("Stream started\n");

		// Capture loop
		i = 0;
		vbuf->type = type;
		while (!ret) {
			ret = video_dequeue(video, &vbuf, K_FOREVER);
			if (ret) {
				printk("Unable to dequeue video buf");
				return 0;
			}

			printk("\rSending frame %d\n", i++);

			// Send video buffer to client
			ret = sendall(client, vbuf->buffer, vbuf->bytesused);
			if (ret && ret != -EAGAIN) {
				printk("\nTCP: Client disconnected %d\n", ret);
				zsock_close(client);
			}

			(void)video_enqueue(video, vbuf);
		} 

		// Stop capture
		if (video_stream_stop(video, type)) {
			printk("Unable to stop video");
			return 0;
		}

		// Flush buffers
		while(!ret) {
			ret = video_dequeue(video, &vbuf, K_NO_WAIT);
		}

	}
}