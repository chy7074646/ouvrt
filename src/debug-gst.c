/*
 * GStreamer debug video output
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <gst/gst.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

struct debug_stream {
	GstElement *pipeline;
	GstElement *appsrc;
	gboolean connected;
};

/*
 * Enables the output of debug frames whenever a GStreamer shmsrc connects to
 * the ouvrtd-gst socket.
 */
static void debug_gst_client_connected(G_GNUC_UNUSED GstElement *sink,
				       G_GNUC_UNUSED gint arg1,
				       gpointer data)
{
	struct debug_stream *gst = data;

	gst->connected = TRUE;
	printf("debug: connected\n");
}

/*
 * Disables the output of debug frames whenever a GStreamer shmsrc disconnects
 * from the ouvrtd-gst socket.
 */
static void debug_gst_client_disconnected(G_GNUC_UNUSED GstElement *sink,
					  G_GNUC_UNUSED gint arg1,
					  gpointer data)
{
	struct debug_stream *gst = data;

	gst->connected = FALSE;
	printf("debug: disconnected\n");
}

/*
 * Enables GStreamer debug output of GRAY8 frames into a shmsink.
 */
struct debug_stream *debug_stream_new(const struct debug_stream_desc *desc)
{
	struct debug_stream *gst;
	GstElement *pipeline, *src, *sink;
	gchar *filename;
	GstCaps *caps;
	guint i;

	for (i = 0; i < 10; i++) {
		filename = g_strdup_printf("/tmp/ouvrtd-gst-%u", i);
		if (!g_file_test(filename, G_FILE_TEST_EXISTS))
			break;
		g_free(filename);
	}

	if (!filename)
		return NULL;

	pipeline = gst_pipeline_new(NULL);

	src = gst_element_factory_make("appsrc", "src");
	sink = gst_element_factory_make("shmsink", "sink");
	if (src == NULL)
		g_error("Could not create appsrc GStreamer element");
	if (sink == NULL)
		g_error("Could not create shmsink GStreamer element");

	caps = gst_caps_new_simple("video/x-raw",
				   "format", G_TYPE_STRING,
					     (desc->format == FORMAT_GRAY) ?
					     "GRAY8" : "YUY2",
				   "framerate", GST_TYPE_FRACTION,
						desc->framerate.numerator,
						desc->framerate.denominator,
				   "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
				   "width", G_TYPE_INT, desc->width,
				   "height", G_TYPE_INT, desc->height,
				   NULL);

	g_object_set(src, "caps", caps, NULL);
	g_object_set(src, "stream-type", 0, NULL);
	g_object_set(sink, "socket-path", filename, NULL);
	g_free(filename);

	gst_caps_unref(caps);

	gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
	gst_element_link_many(src, sink, NULL);

	gst = malloc(sizeof(*gst));
	if (!gst)
		return NULL;
	gst->pipeline = pipeline;
	gst->appsrc = src;
	gst->connected = FALSE;

	g_signal_connect(G_OBJECT(sink), "client-connected",
			 G_CALLBACK(debug_gst_client_connected), gst);
	g_signal_connect(G_OBJECT(sink), "client-disconnected",
			 G_CALLBACK(debug_gst_client_disconnected), gst);

	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	return gst;
}

struct debug_stream *debug_stream_unref(struct debug_stream *gst)
{
	gst_element_set_state(gst->pipeline, GST_STATE_NULL);
	gst_object_unref(gst->pipeline);
	free(gst);

	return NULL;
}

/*
 * Allocates a GstBuffer that wraps the frame and pushes it into the
 * GStreamer pipeline.
 */
void debug_stream_frame_push(struct debug_stream *gst, void *src, size_t size,
			     size_t attach_offset, struct blobservation *ob,
			     dquat *rot, dvec3 *trans, double timestamps[3])
{
	struct ouvrt_debug_attachment *attach;
	unsigned int num;
	GstBuffer *buf;
	int ret;

	if (!gst->connected)
		return;

	if (ob) {
		attach = (struct ouvrt_debug_attachment *)
			 ((char *)src + attach_offset);

		/* Copy blobs and flicker history */
		memcpy(&attach->blobservation, ob, sizeof(*ob));

		/* Copy rotation and translation */
		memcpy(&attach->rot, rot, sizeof(dquat));
		memcpy(&attach->trans, trans, sizeof(dvec3));

		/* Copy raw IMU sensor readings */
		num = debug_imu_fifo_out(attach->imu_samples, 32);
		attach->num_imu_samples = num;

		if (timestamps) {
			memcpy(attach->timestamps, timestamps,
			       4 * sizeof(double));
		}
	}

	buf = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, src,
					  size, 0, size, NULL, NULL);
	if (!buf)
		return;

//	GST_BUFFER_TIMESTAMP(buffer) = ...
//	GST_BUFFER_DURATION(buffer) = ...
	g_signal_emit_by_name(gst->appsrc, "push-buffer", buf, &ret);
	gst_buffer_unref(buf);
}

void debug_stream_init(int *argc, char **argv[])
{
	guint i;

	gst_init(argc, argv);

	for (i = 0; i < 10; i++) {
		gchar *filename = g_strdup_printf("/tmp/ouvrtd-gst-%u", i);
		if (g_file_test(filename, G_FILE_TEST_EXISTS))
			unlink(filename);
		g_free(filename);
	}
}

void debug_stream_deinit(void)
{
	gst_deinit();
}
