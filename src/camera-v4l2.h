/*
 * V4L2 camera class
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __CAMERA_V4L2_H__
#define __CAMERA_V4L2_H__

#include <glib-object.h>
#include <stdint.h>

#include "camera.h"

#define OUVRT_TYPE_CAMERA_V4L2		(ouvrt_camera_v4l2_get_type())
#define OUVRT_CAMERA_V4L2(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_CAMERA_V4L2, \
					 OuvrtCameraV4L2))
#define OUVRT_IS_CAMERA_V4L2(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_CAMERA_V4L2))
#define OUVRT_CAMERA_V4L2_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
					 OUVRT_TYPE_CAMERA, \
					 OuvrtCameraV4L2Class))
#define OUVRT_IS_CAMERA_V4L2_CLASS(klass) \
					(G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_CAMERA_V4L2))
#define OUVRT_CAMERA_V4L2_GET_CLASS(obj) \
					(G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_CAMERA_V4L2, \
					 OuvrtCameraV4L2Class))

typedef struct _OuvrtCameraV4L2		OuvrtCameraV4L2;
typedef struct _OuvrtCameraV4L2Class	OuvrtCameraV4L2Class;
typedef struct _OuvrtCameraV4L2Private	OuvrtCameraV4L2Private;

struct _OuvrtCameraV4L2 {
	OuvrtCamera camera;

	uint32_t pixelformat;

	OuvrtCameraV4L2Private *priv;
};

struct _OuvrtCameraV4L2Class {
	OuvrtCameraClass parent_class;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(OuvrtCameraV4L2, g_object_unref)

GType ouvrt_camera_v4l2_get_type(void);

#endif /* __CAMERA_V4L2_H__ */
