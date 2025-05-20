/*
  Based on Zephyr Video Sample - will be updated
*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/drivers/display.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>

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
	ret = display_set_pixel_format(display_dev, PIXEL_FORMAT_BGR_565);
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
	struct video_buffer *buffers[CONFIG_VIDEO_BUFFER_POOL_NUM_MAX], *vbuf;
	struct video_format fmt;
	struct video_caps caps;
	struct video_frmival frmival;
	struct video_frmival_enum fie;
	enum video_buf_type type = VIDEO_BUF_TYPE_OUTPUT;
	unsigned int frame = 0;
	size_t bsize;
	int i = 0;
	int err;

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
	fmt.pitch = fmt.width * 2;

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
			printk("   %u/%u ", fie.discrete.numerator, fie.discrete.denominator);
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
		buffers[i] = video_buffer_aligned_alloc(bsize, CONFIG_VIDEO_BUFFER_POOL_ALIGN, K_FOREVER);
		if (buffers[i] == NULL) {
			printk("Unable to alloc video buffer");
			return 0;
		}
		buffers[i]->type = type;
		video_enqueue(video_dev, buffers[i]);
	}

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

		printk("Got frame %u! size: %u; timestamp %u ms", frame++, vbuf->bytesused,
		       vbuf->timestamp);

		video_display_frame(display_dev, vbuf, fmt);

		err = video_enqueue(video_dev, vbuf);
		if (err) {
			printk("Unable to requeue video buf");
			return 0;
		}
	}
}