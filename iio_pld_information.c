/*
// Copyright (c) 2015 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <stdlib.h>
#include <stdio.h> 
#include <fcntl.h>
#include <errno.h>
#include "cutils/hashmap.h"
#include "iio_pld_information.h"
#include "iio_common.h"
#include "iio_utils.h"

void setup_properties_from_pld (int sensor_index, int panel, int rotation) {
	/*
	 * Generate suitable order and opt_scale directives from the PLD panel
	 * and rotation codes we got. This can later be superseded by the usual
	 * properties if necessary. Eventually we'll need to replace these
	 * mechanisms by a less convoluted one, such as a 3x3 placement matrix.
	 */

	int x;
	int y;
	int z;
	int xy_swap; 
	int angle; 
	int num_channels;

	x = 1;
	y = 1;
	z = 1;
	xy_swap = 0;
	angle = rotation * 45;
	num_channels = g_sensor_info_iio_ext[sensor_index].num_channels;

	/* Only deal with 3 axis chips for now */
	if (g_sensor_info_iio_ext[sensor_index].num_channels < 3)
		return;

	if (panel == PANEL_BACK) {
		/* Chip placed on the back panel ; negate x and z */
		x = -x;
		z = -z;
	}

	switch (angle) {
		case 90: /* 90° clockwise: negate y then swap x,y */
			xy_swap = 1;
			y = -y;
			break;

		case 180: /* Upside down: negate x and y */
			x = -x;
			y = -y;
			break;

		case 270: /* 90° counter clockwise: negate x then swap x,y */
			x = -x;
			xy_swap = 1;
			break;
	}
	if (xy_swap) {
		g_sensor_info_iio_ext[sensor_index].channel_info[0].index = 1;
		g_sensor_info_iio_ext[sensor_index].channel_info[1].index = 0;
		g_sensor_info_iio_ext[sensor_index].channel_info[2].index = 2;
	}

	g_sensor_info_iio_ext[sensor_index].channel_info[0].opt_scale = x;
	g_sensor_info_iio_ext[sensor_index].channel_info[1].opt_scale = y;
	g_sensor_info_iio_ext[sensor_index].channel_info[2].opt_scale = z;
}


int is_valid_pld (int panel, int rotation)
{
	char buffer[BUFFER_SIZE];

	if (panel != PANEL_FRONT && panel != PANEL_BACK) {
		log_msg_and_exit_on_error(ERROR, "Invalid PLD panel spec: %d\n", panel);
		return -1;
	}

	/* Only deal with 90° rotations for now */
	if (rotation < 0 || rotation > 7 || (rotation & 1)) {
		log_msg_and_exit_on_error(ERROR, "Invalid PLD panel spec: %d\n", rotation);
		return -1;
	}

	return 1;
}

int read_pld_from_sysfs (int sensor_index, int* panel, int* rotation)
{
	char sysfs_path[PATH_MAX];
	int p;
	int r;
	int dev_num;

	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	memset(sysfs_path, '\0', PATH_MAX);

	snprintf(sysfs_path, PATH_MAX, PLD_PANEL_PATH, dev_num);

	if (sysfs_read_int(sysfs_path, &p))
		return -1;

	snprintf(sysfs_path, PATH_MAX, PLD_ROTATION_PATH, dev_num);

	if (sysfs_read_int(sysfs_path, &r))
		return -1;

	if (!is_valid_pld(p, r))
		return -1;

	*panel = p;
	*rotation = r;

	log_msg_and_exit_on_error(VERBOSE, "Sensor %s PLD from sysfs: panel = %d, rotation = %d\n", 
		g_sensor_info_iio_ext[sensor_index].tag, p, r);
	return 0;
}

void decode_placement_information (int sensor_index)
{
	/*
	 * See if we have optional "physical location of device" ACPI tags.
	 * We're only interested in panel and rotation specifiers. Use the
	 * .panel and .rotation properties in priority, and the actual ACPI
	 * values as a second source.
	 */

	int panel;
	int rotation;

	if (read_pld_from_sysfs(sensor_index, &panel, &rotation))
			return; /* No PLD data available */

	/* mapped that to field ordering and scaling mechanisms */
	setup_properties_from_pld(sensor_index, panel, rotation);
}