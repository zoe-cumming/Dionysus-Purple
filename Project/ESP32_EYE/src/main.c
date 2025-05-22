/*
 * Camera, WiFi and IoT setup on esp32s3-eye
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/net/net_ip.h>

#define VIDEO_DEV_SW "VIDEO_SW_GENERATOR"

// WiFi settings
#define WIFI_SSID "Zoe (2)"
#define WIFI_PSK "z0chYo15"

// HTTP GET settings
#define HTTP_HOST "example.com"
#define HTTP_URL "/"

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

static inline int display_setup(const struct device *const display_dev, const uint32_t pixfmt)
{
	struct display_capabilities capabilities;
	int ret = 0;

	printk("Display device: %s", display_dev->name);

	display_get_capabilities(display_dev, &capabilities);

	printk("- Capabilities:");
	printk("  x_resolution = %u, y_resolution = %u, supported_pixel_formats = %u"
		"  current_pixel_format = %u, current_orientation = %u",
		capabilities.x_resolution, capabilities.y_resolution,
		capabilities.supported_pixel_formats, capabilities.current_pixel_format,
		capabilities.current_orientation);

	/* Set display pixel format to match the one in use by the camera */
	switch (pixfmt) {
	case VIDEO_PIX_FMT_RGB565:
		if (capabilities.current_pixel_format != PIXEL_FORMAT_BGR_565) {
			ret = display_set_pixel_format(display_dev, PIXEL_FORMAT_BGR_565);
		}
		break;
	case VIDEO_PIX_FMT_XRGB32:
		if (capabilities.current_pixel_format != PIXEL_FORMAT_ARGB_8888) {
			ret = display_set_pixel_format(display_dev, PIXEL_FORMAT_ARGB_8888);
		}
		break;
	default:
		return -ENOTSUP;
	}

	if (ret) {
		printk("Unable to set display format");
		return ret;
	}

	/* Turn off blanking if driver supports it */
	ret = display_blanking_off(display_dev);
	if (ret == -ENOSYS) {
		printk("Display blanking off not available");
		ret = 0;
	}

	return ret;
}

static inline void video_display_frame(const struct device *const display_dev,
				       const struct video_buffer *const vbuf,
				       const struct video_format fmt)
{
	struct display_buffer_descriptor buf_desc = {
		.buf_size = vbuf->bytesused,
		.width = fmt.width,
		.pitch = buf_desc.width,
		.height = vbuf->bytesused / fmt.pitch,
	};

	display_write(display_dev, 0, vbuf->line_offset, &buf_desc, vbuf->buffer);
}

int main(void)
{
	// Video variables
	struct video_buffer *buffers[CONFIG_VIDEO_BUFFER_POOL_NUM_MAX];
	struct video_buffer *vbuf = &(struct video_buffer){};
	struct video_format fmt;
	struct video_caps caps;
	struct video_frmival frmival;
	struct video_frmival_enum fie;
	enum video_buf_type type = VIDEO_BUF_TYPE_OUTPUT;
	size_t bsize;
	int i = 0;
	int err;

	// WiFi
	struct zsock_addrinfo hints;
    struct zsock_addrinfo *res;
	int sock;
	int ret;

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

	// Get address info
	// memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
	ret = zsock_getaddrinfo(HTTP_HOST, "80", &hints, &res);
    if (ret != 0) {
        printk("Error (%d): Could not perform DNS lookup\r\n", ret);
        return 0;
    }

	// // Create new socket
	// sock = zsock_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    // if (sock < 0) {
    //     printk("Error (%d): Could not create socket\r\n", errno);
    //     return 0;
    // }

	// // Connect the socket
	// ret = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
    // if (ret < 0) {
    //     printk("Error (%d): Could not connect the socket\r\n", errno);
    //     return 0;
    // }

	const struct device *const video_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));

	if (!device_is_ready(video_dev)) {
		printk("%s: video device is not ready", video_dev->name);
		return 0;
	}

	printk("Video device: %s", video_dev->name);

	/* Get capabilities */
	caps.type = type;
	if (video_get_caps(video_dev, &caps)) {
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
	if (video_get_format(video_dev, &fmt)) {
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

	if (video_set_format(video_dev, &fmt)) {
		printk("Unable to set format");
		return 0;
	}

	if (!video_get_frmival(video_dev, &frmival)) {
		printk("- Default frame rate : %f fps",
			1.0 * frmival.denominator / frmival.numerator);
	}

	printk("- Supported frame intervals for the default format:");
	memset(&fie, 0, sizeof(fie));
	fie.format = &fmt;
	while (video_enum_frmival(video_dev, &fie) == 0) {
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

	/* Get supported controls */
	printk("- Supported controls:");

	struct video_ctrl_query cq = {.id = VIDEO_CTRL_FLAG_NEXT_CTRL};

	while (!video_query_ctrl(video_dev, &cq)) {
		video_print_ctrl(video_dev, &cq);
		cq.id |= VIDEO_CTRL_FLAG_NEXT_CTRL;
	}

	/* Set controls */
	struct video_control ctrl = {.id = VIDEO_CID_HFLIP, .val = 1};

	const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display_dev)) {
		printk("%s: display device not ready.", display_dev->name);
		return 0;
	}

	err = display_setup(display_dev, fmt.pixelformat);
	if (err) {
		printk("Unable to set up display");
		return err;
	}

	/* Size to allocate for each buffer */
	if (caps.min_line_count == LINE_COUNT_HEIGHT) {
		bsize = fmt.pitch * fmt.height;
	} else {
		bsize = fmt.pitch * caps.min_line_count;
	}

	/* Alloc video buffers and enqueue for capture */
	for (i = 0; i < ARRAY_SIZE(buffers); i++) {
		/*
		 * For some hardwares, such as the PxP used on i.MX RT1170 to do image rotation,
		 * buffer alignment is needed in order to achieve the best performance
		 */
		buffers[i] = video_buffer_aligned_alloc(bsize, CONFIG_VIDEO_BUFFER_POOL_ALIGN,
							K_FOREVER);
		if (buffers[i] == NULL) {
			printk("Unable to alloc video buffer");
			return 0;
		}
		buffers[i]->type = type;
		video_enqueue(video_dev, buffers[i]);
	}

	ctrl.id = VIDEO_CID_VFLIP;
	video_set_ctrl(video_dev, &ctrl);

	/* Start video capture */
	if (video_stream_start(video_dev, type)) {
		printk("Unable to start capture (interface)");
		return 0;
	}

	printk("Capture started");

	/* Grab video frames */
	vbuf->type = type;
	while (1) {
		err = video_dequeue(video_dev, &vbuf, K_FOREVER);
		if (err) {
			printk("Unable to dequeue video buf");
			return 0;
		}

		video_display_frame(display_dev, vbuf, fmt);

		// Send HTTP data

		err = video_enqueue(video_dev, vbuf);
		if (err) {
			printk("Unable to requeue video buf");
			return 0;
		}
	}
}