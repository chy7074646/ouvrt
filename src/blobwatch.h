/*
 * Blob detection
 * Copyright 2014-2015 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __BLOBWATCH_H__
#define __BLOBWATCH_H__

#include <stdbool.h>
#include <stdint.h>

struct leds;

#define MAX_BLOBS_PER_FRAME  42

struct blob {
	/* center of bounding box */
	uint16_t x;
	uint16_t y;
	int16_t vx;
	int16_t vy;
	/* bounding box */
	uint16_t width;
	uint16_t height;
	uint32_t area;
	uint32_t last_area;
	uint32_t age;
	int16_t track_index;
	uint16_t pattern;
	int8_t led_id;
};

/*
 * Stores all blobs observed in a single frame.
 */
struct blobservation {
	int num_blobs;
	struct blob blobs[MAX_BLOBS_PER_FRAME];
	int tracked_blobs;
	uint8_t tracked[MAX_BLOBS_PER_FRAME];
};

struct blobwatch;

struct blobwatch *blobwatch_new(int width, int height);
void blobwatch_process(struct blobwatch *bw, uint8_t *frame,
		       int width, int height, uint8_t led_pattern_phase,
		       struct leds *leds, struct blobservation **output);
void blobwatch_set_flicker(bool enable);

#endif /* __BLOBWATCH_H__*/
