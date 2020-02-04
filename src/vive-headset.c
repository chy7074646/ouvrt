/*
 * HTC Vive Headset
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <json-glib/json-glib.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "vive-headset.h"
#include "vive-hid-reports.h"
#include "vive-config.h"
#include "vive-firmware.h"
#include "vive-imu.h"
#include "device.h"
#include "hidraw.h"
#include "imu.h"
#include "json.h"
#include "lighthouse.h"
#include "maths.h"
#include "usb-ids.h"

struct _OuvrtViveHeadset {
	OuvrtDevice dev;

	JsonNode *config;
	struct vive_imu imu;
	struct lighthouse_watchman watchman;
};

G_DEFINE_TYPE(OuvrtViveHeadset, ouvrt_vive_headset, OUVRT_TYPE_DEVICE)

/*
 * Downloads the configuration data stored in the headset
 */
static int vive_headset_get_config(OuvrtViveHeadset *self)
{
	char *config_json;
	JsonObject *object;
	struct vive_imu *imu = &self->imu;
	const char *device_class;
	gint64 device_pid, device_vid;
	const char *serial;

	config_json = ouvrt_vive_get_config(&self->dev);
	if (!config_json)
		return -1;

	self->config = json_from_string(config_json, NULL);
	g_free(config_json);
	if (!self->config) {
		g_print("%s: Parsing JSON configuration data failed\n",
			self->dev.name);
		return -1;
	}

	object = json_node_get_object(self->config);

	json_object_get_vec3_member(object, "acc_bias", &imu->acc_bias);
	json_object_get_vec3_member(object, "acc_scale", &imu->acc_scale);

	device_class = json_object_get_string_member(object, "device_class");
	if (strcmp(device_class, "hmd") != 0) {
		g_print("%s: Unknown device class \"%s\"\n", self->dev.name,
			device_class);
	}

	device_pid = json_object_get_int_member(object, "device_pid");
	if (device_pid != PID_VIVE_HEADSET) {
		g_print("%s: Unknown device PID: 0x%04lx\n",
			self->dev.name, device_pid);
	}

	serial = json_object_get_string_member(object, "device_serial_number");
	if (strcmp(serial, self->dev.serial) != 0)
		g_print("%s: Configuration serial number differs: %s\n",
			self->dev.name, serial);

	device_vid = json_object_get_int_member(object, "device_vid");
	if (device_vid != VID_VALVE) {
		g_print("%s: Unknown device VID: 0x%04lx\n",
			self->dev.name, device_vid);
	}

	json_object_get_vec3_member(object, "gyro_bias", &imu->gyro_bias);
	json_object_get_vec3_member(object, "gyro_scale", &imu->gyro_scale);

	json_object_get_lighthouse_config_member(object, "lighthouse_config",
						 &self->watchman.model);
	if (!self->watchman.model.num_points) {
		g_print("%s: Failed to parse Lighthouse configuration\n",
			self->dev.name);
	}

	return 0;
}

static int vive_headset_enable_lighthouse(OuvrtViveHeadset *self,
					  gboolean enable_sensors)
{
	unsigned char buf[5] = { 0x04 };
	int ret;

	/* Enable vsync timestamps, enable/disable sensor reports */
	buf[1] = enable_sensors ? 0x00 : 0x01;
	ret = hid_send_feature_report(self->dev.fd, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	/*
	 * Reset Lighthouse Rx registers? Without this, inactive channels are
	 * not cleared to 0xff.
	 */
	buf[0] = 0x07;
	buf[1] = 0x02;
	return hid_send_feature_report(self->dev.fd, buf, sizeof(buf));
}

/*
 * Decodes the periodic Lighthouse receiver message containing IR pulse
 * timing measurements.
 */
static void vive_headset_decode_pulse_report(OuvrtViveHeadset *self,
					     const void *buf)
{
	const struct vive_headset_lighthouse_pulse_report *report = buf;
	unsigned int i;

	/* The pulses may appear in arbitrary order */
	for (i = 0; i < 9; i++) {
		const struct vive_headset_lighthouse_pulse *pulse;
		uint8_t sensor_id;
		uint16_t duration;
		uint32_t timestamp;

		pulse = &report->pulse[i];

		sensor_id = pulse->id;
		if (sensor_id == 0xff)
			continue;

		timestamp = __le32_to_cpu(pulse->timestamp);
		if (sensor_id == 0xfe) {
			/* TODO: handle vsync timestamp */
			continue;
		}

		if (sensor_id > 31) {
			g_print("%s: unhandled sensor id: %04x\n",
				self->dev.name, sensor_id);
			return;
		}

		duration = __le16_to_cpu(pulse->duration);

		lighthouse_watchman_handle_pulse(&self->watchman,
						 sensor_id, duration,
						 timestamp);
	}
}

/*
 * Opens the IMU and Lighthouse receiver devices, reads the stored
 * configuration and enables the Lighthouse receiver.
 */
static int vive_headset_start(OuvrtDevice *dev)
{
	OuvrtViveHeadset *self = OUVRT_VIVE_HEADSET(dev);
	int ret;

	ret = vive_get_firmware_version(dev);
	if (ret < 0) {
		g_print("%s: Failed to get firmware version\n", dev->name);
		return ret;
	}

	ret = vive_headset_get_config(self);
	if (ret < 0) {
		g_print("%s: Failed to read configuration\n", dev->name);
		return ret;
	}

	ret = vive_headset_enable_lighthouse(self, TRUE);
	if (ret < 0) {
		g_print("%s: Failed to enable Lighthouse Receiver\n",
			dev->name);
		return ret;
	}

	self->watchman.name = dev->name;

	return 0;
}

/*
 * Handles IMU and Lighthouse Receiver messages.
 */
static void vive_headset_thread(OuvrtDevice *dev)
{
	OuvrtViveHeadset *self = OUVRT_VIVE_HEADSET(dev);
	unsigned char buf[64];
	struct pollfd fds[2];
	int ret;

	while (dev->active) {
		fds[0].fd = dev->fds[0];
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		fds[1].fd = dev->fds[1];
		fds[1].events = POLLIN;
		fds[1].revents = 0;

		ret = poll(fds, 2, 1000);
		if (ret == -1) {
			g_print("%s: Poll failure: %d\n", dev->name, errno);
			continue;
		}

		if (ret == 0) {
			g_print("%s: Poll timeout\n", dev->name);
			continue;
		}

		if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) ||
		    (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))) {
			g_print("%s: Disconnected\n", dev->name);
			dev->active = FALSE;
			break;
		}

		if (self->imu.gyro_range == 0.0) {
			ret = vive_imu_get_range_modes(dev, &self->imu);
			if (ret < 0) {
				g_print("%s: Failed to get gyro/accelerometer range modes\n",
					dev->name);
				continue;
			}
		}

		if (fds[0].revents & POLLIN) {
			ret = read(dev->fds[0], buf, sizeof(buf));
			if (ret == -1) {
				g_print("%s: Read error: %d\n", dev->name,
					errno);
				continue;
			}
			if (ret != 52 || buf[0] != VIVE_IMU_REPORT_ID) {
				g_print("%s: Error, invalid %d-byte report 0x%02x\n",
					dev->name, ret, buf[0]);
				continue;
			}

			vive_imu_decode_message(dev, &self->imu, buf, 52);
		}
		if (fds[1].revents & POLLIN) {
			ret = read(dev->fds[1], buf, sizeof(buf));
			if (ret == -1) {
				g_print("%s: Read error: %d\n", dev->name,
					errno);
				continue;
			}
			if (ret == 64 &&
			    buf[0] == VIVE_HEADSET_LIGHTHOUSE_PULSE_REPORT_ID) {
				vive_headset_decode_pulse_report(self, buf);
			} else {
				g_print("%s: Error, invalid %d-byte report 0x%02x\n",
					dev->name, ret, buf[0]);
				continue;
			}
		}
	}
}

/*
 * Nothing to do here.
 */
static void vive_headset_stop(G_GNUC_UNUSED OuvrtDevice *dev)
{
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_headset_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_headset_parent_class)->finalize(object);
}

static void ouvrt_vive_headset_class_init(OuvrtViveHeadsetClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_headset_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_headset_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_headset_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_headset_stop;
}

static void ouvrt_vive_headset_init(OuvrtViveHeadset *self)
{
	self->dev.type = DEVICE_TYPE_HMD;
	self->imu.sequence = 0;
	self->imu.time = 0;
	self->imu.state.pose.rotation.w = 1.0;
	lighthouse_watchman_init(&self->watchman);
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Headset device.
 */
OuvrtDevice *vive_headset_new(G_GNUC_UNUSED const char *devnode)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_VIVE_HEADSET, NULL));
}
