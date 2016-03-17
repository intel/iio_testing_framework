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
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include "cutils/hashmap.h"
#include "iio_enumeration.h"
#include "iio_utils.h"
#include "iio_pld_information.h"
#include "iio_set_trigger.h"

int g_sensor_iio_count  = 0;

void add_sensor(int dev_num, int s, int mode) {
	
	char sysfs_path[PATH_MAX];
	const char* prefix;
	int index;
	int c;
	int num_channels;
	float scale;
	float offset;
	
	if (g_sensor_iio_count == MAX_SENSORS) {
		log_msg_and_exit_on_error(DEBUG, "Too many sensors!\n");
		return;
	}
	memset(sysfs_path, '\0', PATH_MAX);

	/* Read name attribute, if available */
	snprintf(sysfs_path, PATH_MAX, NAME_PATH, dev_num);
	sysfs_read_str(sysfs_path, g_sensor_info_iio_ext[s].internal_name, MAX_NAME_SIZE);

	if (g_sensor_info_iio_ext[s].internal_name[0] == '\0') {
		/*
		 * In case the kernel-mode driver doesn't expose a name for
		 * the iio device, the device won't be added to list sensors
		 */
		log_msg_and_exit_on_error(DEBUG, "Device%d has no name and it won't be "
			"added to sensors list!\n", dev_num);
		return;
	}

	g_sensor_info_iio_ext[s].dev_num	= dev_num;
	g_sensor_info_iio_ext[s].mode		= mode;
	g_sensor_info_iio_ext[s].discovered = 1;

	prefix = g_sensor_info_iio_ext[s].tag;
	num_channels = g_sensor_info_iio_ext[s].num_channels;

	if(mode == MODE_TRIGGER) {
		/* Read channel specific index if any*/
		for (c = 0; c < num_channels; c++) {
			snprintf(sysfs_path, PATH_MAX, CHANNEL_PATH "%s", dev_num, 
				g_sensor_info_iio_ext[s].channel_descriptor[c].index_path);
			if (!sysfs_read_int(sysfs_path, &index)) {
				g_sensor_info_iio_ext[s].channel_info[c].index = index;
				log_msg_and_exit_on_error(VERBOSE, "Index path:%s " "channel index:%d dev_num:%d\n",
					sysfs_path, index, dev_num);
			}
		}

		/* Read timestamp specific index if any*/
		snprintf(sysfs_path, PATH_MAX, TIMESTAMP_INDEX_PATH, dev_num);
		if (!sysfs_read_int(sysfs_path, &index)) {
			g_sensor_info_iio_ext[s].timestamp.index = index;
			log_msg_and_exit_on_error(VERBOSE, "Index path:%s " "timestamp index:%d dev_num:%d\n",
				sysfs_path, index, dev_num);
		}

		/* Read and print data rate */
		snprintf(sysfs_path, PATH_MAX, SENSOR_SAMPLING_PATH, dev_num, g_sensor_info_iio_ext[s].tag);
		if (sysfs_read_float(sysfs_path, &g_sensor_info_iio_ext[s].data_rate) == -1) {
			snprintf(sysfs_path, PATH_MAX, DEVICE_SAMPLING_PATH, dev_num);
			if (sysfs_read_float(sysfs_path, &g_sensor_info_iio_ext[s].data_rate) == -1) {
				log_msg_and_exit_on_error(VERBOSE, "Data rate for device%d: %f\n", dev_num,
					g_sensor_info_iio_ext[s].data_rate);
			}
			else{
				log_msg_and_exit_on_error(DEBUG, "%s: Cannot read frequency for device%d!.\n", sysfs_path, dev_num);
			}
		}
		
		/* enable channels */
		for (c=0; c < num_channels; c++) {
			snprintf(sysfs_path, PATH_MAX, CHANNEL_PATH "%s", dev_num, 
				g_sensor_info_iio_ext[s].channel_descriptor[c].en_path);
			sysfs_write_int(sysfs_path, 1);
		}
		/* enable timestamp channel */
		snprintf(sysfs_path, PATH_MAX, TIMESTAMP_ENABLE_PATH, dev_num);
		sysfs_write_int(sysfs_path, 1);

	}

	/* Set offset value if any */
	snprintf(sysfs_path, PATH_MAX, SENSOR_OFFSET_PATH, dev_num, prefix);
	if(sysfs_read_float(sysfs_path, &g_sensor_info_iio_ext[s].offset) == -1)
		g_sensor_info_iio_ext[s].offset = 0;
	else 
		log_msg_and_exit_on_error(VERBOSE, "Offset path:%s offset:%g dev_num:%d\n", 
			sysfs_path, g_sensor_info_iio_ext[s].offset, dev_num);
	
	/* Set general scale if any */
	snprintf(sysfs_path, PATH_MAX, SENSOR_SCALE_PATH, dev_num, prefix);
	if (!sysfs_read_float(sysfs_path, &scale)) {
		g_sensor_info_iio_ext[s].scale = scale;
		log_msg_and_exit_on_error(VERBOSE, "Scale path:%s scale:%g dev_num:%d\n", 
			sysfs_path, scale, dev_num);
	} 
	else {
			g_sensor_info_iio_ext[s].scale = 1;
				/* Read channel specific scale if any*/
				for (c = 0; c < num_channels; c++) {
					snprintf(sysfs_path, PATH_MAX, BASE_PATH "%s", dev_num,
					   g_sensor_info_iio_ext[s].channel_descriptor[c].scale_path);
					if (!sysfs_read_float(sysfs_path, &scale)) {
						g_sensor_info_iio_ext[s].channel_info[c].scale = scale;
						g_sensor_info_iio_ext[s].scale = 0;
						log_msg_and_exit_on_error(VERBOSE, "Scale path:%s channel scale:%g dev_num:%d\n", 
							sysfs_path, scale, dev_num);
					}
				}
	}

	/* Set pld information */
	decode_placement_information(s);
	g_sensor_iio_count++;
}

/* check if sensor is in polling mode */
void check_trig_sensors (int i, char *sysfs_file, char mapped[])
{

	if (g_sensor_info_iio_ext[i].channel_descriptor[0].en_path &&
			!strcmp(sysfs_file, g_sensor_info_iio_ext[i].channel_descriptor[0].en_path)) {
		mapped[i] = 1;
		return;
	}
}

/* check if sensor is in triggered mode */
void check_poll_sensors (int i, char *sysfs_file, char mapped[])
{
	int c;

	for (c = 0; c < g_sensor_info_iio_ext[i].num_channels; c++)
		if (!strcmp(sysfs_file, g_sensor_info_iio_ext[i].channel_descriptor[c].raw_path) ||
			!strcmp(sysfs_file, g_sensor_info_iio_ext[i].channel_descriptor[c].input_path)) {
			mapped[i] = 1;
			break;
		}
}
/* discover sensors types */
void discover_sensors(int dev_num, char *sysfs_base_path, char mapped[],
			  void (*discover_sensor)(int, char*, char*))
{
	char sysfs_dir[PATH_MAX];
	DIR *dir;
	struct dirent *d;
	int i;

	memset(sysfs_dir, '\0', PATH_MAX);
	snprintf(sysfs_dir, PATH_MAX, sysfs_base_path, dev_num);

	dir = opendir(sysfs_dir);
	if (!dir) {
		return;
	}

	/* Enumerate entries in this iio device's base folder */
	while ((d = readdir(dir))) {
		if (!strncmp(d->d_name, ".", 1) || !strncmp(d->d_name, "..", 2))
			continue;

		/* If the name matches a catalog entry, flag it */
		for (i = 0; i < g_sensor_info_size; i++) {

			/* No discovery for virtual sensors */
			if (g_sensor_info_iio_ext[i].is_virtual)
				continue;
			discover_sensor(i, d->d_name, mapped);
		}

	}

	closedir(dir);
}


int find_buffer_and_scan_elem(int dev_num) {

	DIR *buffer_dir;
	DIR *scan_elements_dir;
	char buffer_path[PATH_MAX];
	char scan_elements_path[PATH_MAX];


	memset(buffer_path, '\0', PATH_MAX);
	memset(scan_elements_path, '\0', PATH_MAX);
	snprintf(buffer_path, PATH_MAX, BUFFER_PATH, dev_num);
	snprintf(scan_elements_path, PATH_MAX, SCAN_ELEMENTS_PATH, dev_num);

	buffer_dir = opendir(buffer_path);
	scan_elements_dir = opendir(scan_elements_path);

	if (!buffer_dir && !scan_elements_dir) {
		return 0;
	}

	return 1;	
}


/*
** This table mappeds syfs entries in scan_elements directories to sensor types,
** and will also be used to determine other sysfs names as well as the iio
** device number associated to a specific sensor.
*/

sensor_info_iio_ext_t g_sensor_info_iio_ext[] = {
	{
		tag		: "accel",
		type		: SENSOR_TYPE_ACCELEROMETER,
		num_channels	: 3,
		trigger_nr : 0,
		found_implicit_trigger : 0,
		hr_trigger_nr : -1,
		last_timestamp : -1,
		discovered : 0,
		channel_descriptor  : {
			{ DECLARE_NAMED_CHANNEL("accel", "x") },
			{ DECLARE_NAMED_CHANNEL("accel", "y") },
			{ DECLARE_NAMED_CHANNEL("accel", "z") },
		}
	},
	{
		tag		: "anglvel",
		type		: SENSOR_TYPE_GYROSCOPE,
		num_channels	: 3,
		trigger_nr : 0,
		found_implicit_trigger : 0,
		hr_trigger_nr : -1,
		last_timestamp : -1,
		discovered : 0,
		channel_descriptor  : {
			{ DECLARE_NAMED_CHANNEL("anglvel", "x") },
			{ DECLARE_NAMED_CHANNEL("anglvel", "y") },
			{ DECLARE_NAMED_CHANNEL("anglvel", "z") },
		}
	},
	{
		.tag		= "magn",
		.type		= SENSOR_TYPE_MAGNETIC_FIELD,
		.num_channels	= 3,
		.trigger_nr = 0,
		.found_implicit_trigger = 0,
		.hr_trigger_nr = -1,
		.last_timestamp = -1,
		.discovered = 0,
		.channel_descriptor  = {
			{ DECLARE_NAMED_CHANNEL("magn", "x") },
			{ DECLARE_NAMED_CHANNEL("magn", "y") },
			{ DECLARE_NAMED_CHANNEL("magn", "z") },
		}
	},
	{
		.tag		= "intensity",
		.type		= SENSOR_TYPE_INTERNAL_INTENSITY,
		.num_channels	= 1,
		.trigger_nr = 0,
		.found_implicit_trigger = 0,
		.hr_trigger_nr = -1,
		.last_timestamp = -1,
		.discovered = 0,
		.channel_descriptor  = {
			{ DECLARE_NAMED_CHANNEL("intensity", "both") },
		}
	},
	{
		.tag		= "illuminance",
		.type		= SENSOR_TYPE_INTERNAL_ILLUMINANCE,
		.num_channels	= 1,
		.trigger_nr = 0,
		.found_implicit_trigger = 0,
		.hr_trigger_nr = -1,
		.last_timestamp = -1,
		.discovered = 0,
		.channel_descriptor  = {
			{ DECLARE_GENERIC_CHANNEL("illuminance") },
		}
	},
	{
		.tag		= "temp",
		.type		= SENSOR_TYPE_AMBIENT_TEMPERATURE,
		.num_channels	= 1,
		.trigger_nr = 0,
		.found_implicit_trigger = 0,
		.hr_trigger_nr = -1,
		.last_timestamp = -1,
		.discovered = 0,
		.channel_descriptor  = {
			{ DECLARE_GENERIC_CHANNEL("temp") },
		}
	}
};

int g_sensor_info_size = ARRAY_SIZE(g_sensor_info_iio_ext);

void enumerate_sensors (void)
{
	char trig_sensors[g_sensor_info_size];
	char poll_sensors[g_sensor_info_size];
	int dev_num;
	int i;

	log_msg_and_exit_on_error(VERBOSE, "enumerate_sensors\n");

	for (dev_num = 0; dev_num < MAX_DEVICES; dev_num++) {
		memset(trig_sensors, 0, g_sensor_info_size);
		memset(poll_sensors, 0, g_sensor_info_size);
		if(find_buffer_and_scan_elem(dev_num) == 0) {
			discover_sensors(dev_num, BASE_PATH, poll_sensors, check_poll_sensors);
		}
		else{
			discover_sensors(dev_num, CHANNEL_PATH, trig_sensors, check_trig_sensors);
		}
		
		for (i=0; i< g_sensor_info_size; i++) {
			if (trig_sensors[i]) {
				add_sensor(dev_num, i, MODE_TRIGGER);
				continue;
			}
			else if(poll_sensors[i]) {
				add_sensor(dev_num, i, MODE_POLL);
				continue;
			}

		}
	}

	log_msg_and_exit_on_error(VERBOSE, "Discovered %d sensors\n", g_sensor_iio_count);

	/* Set up default - as well as custom - trigger names */
	select_trigger();
}



