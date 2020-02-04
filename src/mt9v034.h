/*
 * Aptina MT9V034 Image Sensor initialization
 * Copyright 2014 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __MT9V034_H__
#define __MT9V034_H__

int mt9v034_sensor_setup(int fd);
int mt9v034_sensor_enable_sync(int fd);
int mt9v034_sensor_disable_sync(int fd);

#endif /* __MT9V034_H__ */
