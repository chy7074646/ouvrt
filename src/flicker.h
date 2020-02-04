/*
 * LED pattern detection and identification
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __FLICKER_H__
#define __FLICKER_H__

#include <stdbool.h>
#include <stdint.h>

struct blob;
struct leds;

void flicker_process(struct blob *blobs, int num_blobs,
		     uint8_t led_pattern_phase, struct leds *leds);

#endif /* __BLOBWATCH_H__*/
