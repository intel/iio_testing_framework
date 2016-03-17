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
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>
#include "cutils/hashmap.h"
#include "iio_set_trigger.h"
#include "iio_utils.h"
#include "iio_common.h"

/* list triggers that are exposed */
void list_triggers() {
	int i, j;
	char buffer[BUFFER_SIZE];

	for (i = 0; i < g_sensor_info_size; ++i) {
		if(!g_sensor_info_iio_ext[i].discovered)
			continue;
		for (j = 0; j < g_sensor_info_iio_ext[i].trigger_nr; ++j) {
			log_msg_and_exit_on_error(VERBOSE, "Found trigger %s for device%d\n",
				g_sensor_info_iio_ext[i].triggers[j], g_sensor_info_iio_ext[i].dev_num);
		}
	}
}
int enable_trigger(int dev_num, const char* trigger_val) {
	char sysfs_path[PATH_MAX];
	int ret;
	int attempts;

	ret = -1;
	attempts = 5;
	
	memset(sysfs_path, '\0', PATH_MAX);
	snprintf(sysfs_path, PATH_MAX, TRIGGER_PATH, dev_num);

	log_msg_and_exit_on_error(VERBOSE, "Setting %s to %s.\n", sysfs_path, trigger_val);

	while (ret == -1 && attempts) {
		ret = sysfs_write_str(sysfs_path, trigger_val);
		attempts--;
	}
	if (ret == -1) {
		log_msg_and_exit_on_error(DEBUG, "Setting %s to %s was a failure.\n", sysfs_path, trigger_val);
	}
	else{
		log_msg_and_exit_on_error(VERBOSE, "Setting %s to %s was a success.\n", sysfs_path, trigger_val);
	}
	return ret;
}
void propose_new_trigger (int s, char trigger_name[MAX_NAME_SIZE], int hr_trigger_nr) {
	/*
	** A new trigger has been enumerated for this sensor. Check if it makes sense 
	** to use it over the currently selected one and select it if it is so. 
	** The format is something like sensor_name-dev0.
	*/
	int trigger_nr;
	int sensor_name_len;

	sensor_name_len = strnlen(g_sensor_info_iio_ext[s].internal_name, MAX_NAME_SIZE);
	trigger_nr = g_sensor_info_iio_ext[s].trigger_nr;
	strncpy(g_sensor_info_iio_ext[s].triggers[trigger_nr], trigger_name, strlen(trigger_name));
	g_sensor_info_iio_ext[s].trigger_nr++;


	const char *suffix = trigger_name + sensor_name_len + 1;

	/* dev is the default, and lowest priority; no need to update */
	if (!memcmp(suffix, "dev", 3))
		g_sensor_info_iio_ext[s].found_implicit_trigger = 1;

	if (!memcmp(suffix, "hr-dev", 6)) 
		g_sensor_info_iio_ext[s].hr_trigger_nr = hr_trigger_nr;

}
int create_hrtimer_trigger(int s, int hr_trigger_nr) {
	struct stat dir_status;
	char buf[MAX_NAME_SIZE];
	char hrtimer_path[PATH_MAX];
	char hrtimer_name[MAX_NAME_SIZE];

	memset(buf, '\0', MAX_NAME_SIZE);
	memset(hrtimer_path, '\0', PATH_MAX);
	memset(hrtimer_name, '\0', MAX_NAME_SIZE);

	snprintf(buf, MAX_NAME_SIZE, "hrtimer-%s-hr-dev%d",
		g_sensor_info_iio_ext[s].internal_name, g_sensor_info_iio_ext[s].dev_num);
	snprintf(hrtimer_name, MAX_NAME_SIZE, "%s-hr-dev%d",
		g_sensor_info_iio_ext[s].internal_name, g_sensor_info_iio_ext[s].dev_num);
	snprintf(hrtimer_path, PATH_MAX, "%s%s", CONFIGFS_TRIGGER_PATH, buf);

	/* Get parent dir status */
	if (stat(CONFIGFS_TRIGGER_PATH, &dir_status))
		return -1;

	/* Create hrtimer with the same access rights as it's parent */
	if (mkdir(hrtimer_path, dir_status.st_mode))
		if (errno != EEXIST)
			return -1;
	g_sensor_info_iio_ext[s].hr_trigger_nr = hr_trigger_nr;
	propose_new_trigger(s, hrtimer_name, hr_trigger_nr);	
	strncpy (g_sensor_info_iio_ext[s].init_trigger_name, hrtimer_name, MAX_NAME_SIZE);
	log_msg_and_exit_on_error(VERBOSE, "Device%d has trigger %s\n",
		g_sensor_info_iio_ext[s].dev_num, g_sensor_info_iio_ext[s].init_trigger_name);	
	
	return 0;
}

void update_sensor_matching_trigger_name (char name[MAX_NAME_SIZE], int trigger_nr) {
	/*
	** Check if we have a sensor matching the specified trigger name, 
	** which should then begin with the sensor name, and end with a number
	** equal to the iio device number the sensor is associated to. If so, 
	** update the string we're going to write to trigger/current_trigger
	** when enabling this sensor.
	*/

	int s;
	int dev_num;
	int len;
	char* cursor;
	int sensor_name_len;

	/*
	** First determine the iio device number this trigger refers to. 
	** We expect the last few characters (typically one) of the trigger name
	** to be this number, so perform a few checks.
	*/
	len = strnlen(name, MAX_NAME_SIZE);
	if (len < 2)
		return;

	cursor = name + len - 1;

	if (!isdigit(*cursor))
		return;

	while (len && isdigit(*cursor)) {
		len--;
		cursor--;
	}

	dev_num = atoi(cursor+1);

	/* See if that matches a sensor */
	for (s = 0; s < g_sensor_info_size; s++) {
		if (!g_sensor_info_iio_ext[s].discovered)
			break;
		
		if (g_sensor_info_iio_ext[s].dev_num == dev_num) {

			sensor_name_len = strnlen(g_sensor_info_iio_ext[s].internal_name, MAX_NAME_SIZE);

			if (!strncmp(name, g_sensor_info_iio_ext[s].internal_name, sensor_name_len))
				/* Switch to new trigger if appropriate */
				propose_new_trigger(s, name, trigger_nr);
		}
	}	
}
void select_trigger(void) {
	int s;
	char sysfs_path[PATH_MAX];
	char trigger_name[MAX_NAME_SIZE];
	int trigger;

	memset(sysfs_path, '\0', PATH_MAX);
	memset(trigger_name, '\0', MAX_NAME_SIZE);

	/* Now have a look to /sys/bus/iio/devices/triggerX entries */
	for (trigger = 0; trigger < MAX_TRIGGERS; trigger++) {
		snprintf(sysfs_path, PATH_MAX, TRIGGER_FILE_PATH, trigger);
		if(sysfs_read_str(sysfs_path, trigger_name, MAX_NAME_SIZE) < 0)
			break;

		/* Record initial and any-motion triggers names */
		update_sensor_matching_trigger_name(trigger_name, trigger);
	}

	/* By default, use the name-dev convention that most drivers use*/ 
	for (s = 0; s < g_sensor_info_size; s++) {
		if (!g_sensor_info_iio_ext[s].discovered)
			continue;
		if(g_sensor_info_iio_ext[s].mode == MODE_POLL)
			continue;		
		else if(g_sensor_info_iio_ext[s].found_implicit_trigger == 1) {
			snprintf(g_sensor_info_iio_ext[s].init_trigger_name, MAX_NAME_SIZE, "%s-dev%d",
				g_sensor_info_iio_ext[s].internal_name, g_sensor_info_iio_ext[s].dev_num);
			log_msg_and_exit_on_error(VERBOSE, "Device%d has trigger %s\n",
				g_sensor_info_iio_ext[s].dev_num, g_sensor_info_iio_ext[s].init_trigger_name);	
			continue;
			
		
		}
		else if(g_sensor_info_iio_ext[s].trigger_nr > 0) {
			strncpy(g_sensor_info_iio_ext[s].init_trigger_name, g_sensor_info_iio_ext[s].triggers[0],
				strlen(g_sensor_info_iio_ext[s].triggers[0]));
			log_msg_and_exit_on_error(VERBOSE, "Device%d has trigger %s\n",
				g_sensor_info_iio_ext[s].dev_num, g_sensor_info_iio_ext[s].init_trigger_name);	
			continue;
		}
		else{
			log_msg_and_exit_on_error(VERBOSE, "Device%d has no trigger!\n",
				g_sensor_info_iio_ext[s].dev_num);
			create_hrtimer_trigger(s, trigger);
			trigger++;

		}
	}

}
