/*
 * Pose estimation using OpenCV
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __OPENCV_H__
#define __OPENCV_H__

#include "maths.h"

#if HAVE_OPENCV
void estimate_initial_pose(struct blob *blobs, int num_blobs,
			   vec3 *leds, int num_leds,
			   dmat3 *camera_matrix, double dist_coeffs[5],
			   dquat *rot, dvec3 *trans, bool use_extrinsic_guess);
#else
static inline
void estimate_initial_pose(struct blob *blobs, int num_blobs,
			   vec3 *leds, int num_leds,
			   dmat3 *camera_matrix, double dist_coeffs[5],
			   dquat *rot, dvec3 *trans, bool use_extrinsic_guess)
{
	(void)blobs;
	(void)num_blobs;
	(void)leds;
	(void)num_leds;
	(void)camera_matrix;
	(void)dist_coeffs;
	(void)rot;
	(void)trans;
	(void)use_extrinsic_guess;
}
#endif /* HAVE_OPENCV */

#endif /* __OPENCV_H__ */
