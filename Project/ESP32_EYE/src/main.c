#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/drivers/display.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/net_ip.h>

#define VIDEO_DEV_SW "VIDEO_SW_GENERATOR"
#define MY_PORT 5000
#define MAX_CLIENT_QUEUE 1

#define STACKSIZE 4096

// WiFi settings
#define WIFI_SSID "Zoe"
#define WIFI_PSK "z0chYo15"

// Define WiFi variables
#define NET_EVENT_WIFI_MASK \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT | \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED)

static struct net_if *sta_iface;
static struct wifi_connect_req_params sta_config;
static struct net_mgmt_event_callback cb;

// Define the video buffers 
struct video_buffer *buffers[CONFIG_VIDEO_BUFFER_POOL_NUM_MAX];
struct video_buffer *vbuf = &(struct video_buffer){};

// Define variables to be sent to client
#define BUFFER_SIZE (CONFIG_VIDEO_FRAME_WIDTH * CONFIG_VIDEO_FRAME_HEIGHT * 2)
// __attribute__ ((section (".ext_ram.bss"))) uint8_t frame_copy[BUFFER_SIZE]; // Store in PSRAM
__attribute__ ((section (".ext_ram.bss"))) uint8_t *frame_copy;
static size_t frame_size = 0;

// Polling signal for image data transmission
struct k_poll_signal tx_signal;
K_FIFO_DEFINE(tx_fifo);

/*
 * WiFi callback function
 */
static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
			       struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT: {
		printk("Connected to %s\n", WIFI_SSID);
		break;
	}
	case NET_EVENT_WIFI_DISCONNECT_RESULT: {
		printk("Disconnected from %s\n", WIFI_SSID);
		break;
	}
	default:
		break;
	}
}

/*
 * Connect to a WiFi station
 */
static int connect_to_wifi(void)
{
	if (!sta_iface) {
		printk("STA: interface not initialized\n");
		return -EIO;
	}

	// Configure the station parameters 
	sta_config.ssid = (const uint8_t *)WIFI_SSID;
	sta_config.ssid_length = strlen(WIFI_SSID);
	sta_config.psk = (const uint8_t *)WIFI_PSK;
	sta_config.psk_length = strlen(WIFI_PSK);
	sta_config.security = WIFI_SECURITY_TYPE_PSK;
	sta_config.channel = WIFI_CHANNEL_ANY;
	sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;

	printk("Connecting to SSID: %s\n", sta_config.ssid);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config,
			   sizeof(struct wifi_connect_req_params));
	if (ret) {
		printk("Unable to Connect to (%s)\n", WIFI_SSID);
	}

	return ret;
}

/*
 * Initialise the LCD screen
 */
static inline int display_setup(const struct device *const display_dev, const uint32_t pixfmt)
{
	struct display_capabilities capabilities;
	int ret = 0;

	printk("Display device: %s", display_dev->name);

	display_get_capabilities(display_dev, &capabilities);

	// Set display pixel format to be same as camera
	ret = display_set_pixel_format(display_dev, PIXEL_FORMAT_BGR_565);
	if (ret) {
		printk("Unable to set display format\n");
		return ret;
	}

	// Turn off blanking
	ret = display_blanking_off(display_dev);
	if (ret == -ENOSYS) {
		printk("Display blanking off not available\n");
		ret = 0;
	}

	return ret;
}

/*
 * Display video stream on LCD screen
 */
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

/*
 * Send READY message and image data to client
 */
static ssize_t sendall(int sock, const void *buf, size_t len)
{
	zsock_send(sock, "READY", 5, 0);
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

/*
 * Set up the camera and LCD screen
 * Continually stream camera data to LCD screen
 * Populate frame_copy and frame_size when polling signal raised
 */
void camera_thread(void)
{
	static int i, ret;
	static struct video_format fmt;
	static struct video_caps caps;
	struct video_frmival frmival;
	struct video_frmival_enum fie;
	enum video_buf_type type = VIDEO_BUF_TYPE_OUTPUT;
	size_t bsize;

	// Initialise polling signal
	struct k_poll_event events[1] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &tx_signal)
	};
	int signaled, result;

	// Initialise the camera 
	const struct device *const video = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));

	if (!device_is_ready(video)) {
		printk("%s: video device not ready\n", video->name);
		return;
	}

	// Get capabilities
	caps.type = type;
	if (video_get_caps(video, &caps)) {
		printk("Unable to retrieve video capabilities\n");
		return;
	}

	// Get default format
	fmt.type = type;
	if (video_get_format(video, &fmt)) {
		printk("Unable to retrieve video format\n");
		return;
	}

	// Set the frame width and height (240 x 240)
	fmt.height = CONFIG_VIDEO_FRAME_HEIGHT;
	fmt.width = CONFIG_VIDEO_FRAME_WIDTH;

	// Set the pixel format
	if (strcmp(CONFIG_VIDEO_PIXEL_FORMAT, "")) {
		fmt.pixelformat = VIDEO_FOURCC_FROM_STR(CONFIG_VIDEO_PIXEL_FORMAT);
	}

	printk("- Video format: %s %ux%u",
		VIDEO_FOURCC_TO_STR(fmt.pixelformat), fmt.width, fmt.height);

	if (video_set_format(video, &fmt)) {
		printk("Unable to set format\n");
		return;
	}

	// Frame rate
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

	// Initialise the LCD screen
	const struct device *const display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display_dev)) {
		printk("%s: display device not ready\n", display_dev->name);
		return;
	}

	ret = display_setup(display_dev, fmt.pixelformat);
	if (ret) {
		printk("Unable to set up display\n");
		return;
	}

	// Size to allocate for each buffer
	if (caps.min_line_count == LINE_COUNT_HEIGHT) {
		bsize = fmt.pitch * fmt.height;
	} else {
		bsize = fmt.pitch * caps.min_line_count;
	}
	printk("Size: %d\n", bsize);

	// Allocate video buffers
	for (i = 0; i < ARRAY_SIZE(buffers); i++) {
		buffers[i] = video_buffer_aligned_alloc(bsize, CONFIG_VIDEO_BUFFER_POOL_ALIGN,
							K_FOREVER);
		if (buffers[i] == NULL) {
			printk("Unable to alloc video buffer\n");
			return;
		}
		buffers[i]->type = type;
		video_enqueue(video, buffers[i]);
	}

	// Start the video stream
	if (video_stream_start(video, type)) {
		printk("Unable to start video\n");
		return;
	}
	printk("Stream started\n");
	vbuf->type = type;
	i = 0;

	while (1) {
		// Dequeue image buffers
		k_poll(events, 1, K_NO_WAIT);
		k_poll_signal_check(&tx_signal, &signaled, &result);
		ret = video_dequeue(video, &vbuf, K_SECONDS(20));
		if (ret) {
			printk("Unable to dequeue video buf\n");
			return;
		}

		// Re-populate frame_copy and frame_size when polling signal raised
		if (signaled && (result == 0x01)) {
			k_poll_signal_reset(&tx_signal);
		 	events[0].state = K_POLL_STATE_NOT_READY;
			// memcpy(frame_copy, vbuf->buffer, vbuf->bytesused);
			// frame_size = vbuf->bytesused;
			frame_copy = vbuf->buffer;
			frame_size = vbuf->bytesused;
		}

		// Display image on LCD
		video_display_frame(display_dev, vbuf, fmt);

		// Enqeue image buffers
		ret = video_enqueue(video, vbuf);
		if (ret) {
			printk("Unable to requeue video buf\n");
			return;
		}
	}
}

/*
 * Set up the TCP socket
 * Accept client connections
 * Stream image data to client 
 */
void network_thread(void)
{
	static struct sockaddr_in addr, client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	static int ret, sock, client;

	// Prepare network
	(void)memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(MY_PORT);

	// Create TCP socket
	sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		printk("Failed to create TCP socket: %d\n", errno);
		return;
	}

	ret = zsock_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		printk("Failed to bind TCP socket: %d\n", errno);
		zsock_close(sock);
		return;
	}

	ret = zsock_listen(sock, MAX_CLIENT_QUEUE);
	if (ret < 0) {
		printk("Failed to listen on TCP socket: %d\n", errno);
		zsock_close(sock);
		return;
	}

	while (1) {
		// Accept client connections 
		printk("TCP: Waiting for client...\n");

		client = zsock_accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
		if (client < 0) {
			printk("Failed to accept: %d\n", errno);
			return;
		}

		printk("TCP: Accepted connection\n");

		while (1) {
			// Send image data to client
			ret = sendall(client, frame_copy, frame_size);
			if (ret && ret != -EAGAIN) {
				printk("\nTCP: Client disconnected %d\n", ret);
				zsock_close(client);
				break;
			}
			// Re-populate frame_copy and frame size in camera thread
			k_poll_signal_raise(&tx_signal, 0x01);
		} 
	}
}

int main(void) {
	// Initialise polling signal for data transmission
	k_poll_signal_init(&tx_signal);
	k_poll_signal_raise(&tx_signal, 0x01);

	// Initialise WiFi connection 
	net_mgmt_init_event_callback(&cb, wifi_event_handler, NET_EVENT_WIFI_MASK);
	net_mgmt_add_event_callback(&cb);
	sta_iface = net_if_get_wifi_sta();
	connect_to_wifi();
}

// Define camera and network (TCP) threads
 K_THREAD_DEFINE(camera_id, STACKSIZE, camera_thread, NULL, NULL, NULL, 1, 0, 0);
 K_THREAD_DEFINE(network_id, STACKSIZE, network_thread, NULL, NULL, NULL, 3, 0, 0);