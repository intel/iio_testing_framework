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
#include <ctype.h>
#include "cutils/hashmap.h"
#include "iio_control_frequency.h"
#include "iio_enumeration.h"
#include "iio_utils.h"
#include "iio_control.h"

float get_cdd_freq (int sensor_index, int must) {
	switch (g_sensor_info_iio_ext[sensor_index].type) {
		case SENSOR_TYPE_ACCELEROMETER:
			return (must ? 100 : 200); /* must 100 Hz, should 200 Hz, CDD compliant */

		case SENSOR_TYPE_GYROSCOPE:
			return (must ? 200 : 200); /* must 200 Hz, should 200 Hz, CDD compliant */

		case SENSOR_TYPE_MAGNETIC_FIELD:
			return (must ? 10 : 50);   /* must 10 Hz, should 50 Hz, CDD compliant */

		case SENSOR_TYPE_AMBIENT_TEMPERATURE:
			return (must ? 1 : 2);     /* must 1 Hz, should 2Hz, not mentioned in CDD */

		default:
			return 1; /* Use 1 Hz by default, e.g. for proximity */
	}
}

float get_standard_deviation_value(int sensor_index) {
	switch (g_sensor_info_iio_ext[sensor_index].type) {
		case SENSOR_TYPE_ACCELEROMETER:
			return 0.05;

		case SENSOR_TYPE_MAGNETIC_FIELD:
			return CONVERT_MICROTESLA_TO_GAUSS(0.5);

		default:
			return -1; 
	}
}

/* select closest frequency to an available one */
float select_closest_freq(int sensor_index, float required_freq) {
	char sysfs_path[PATH_MAX];
	char available_frequencies[BUFFER_SIZE];
	char* cursor;
	const char* tag;
	float set_freq;
	float old_freq;
	float current_freq;
	float max_freq;
	int dev_num;
	if (required_freq <= 0) {
		log_msg_and_exit_on_error(ERROR, "Can't set a negative or null data rate!\n");
		set_test_state(FAILED);
		return -1;
	}

	memset(sysfs_path, '\0', PATH_MAX);
	memset(available_frequencies, '\0', BUFFER_SIZE);
	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	tag = g_sensor_info_iio_ext[sensor_index].tag;
	max_freq = (float)get_cdd_freq(sensor_index, 0);

	snprintf(sysfs_path, PATH_MAX, SENSOR_SAMPLING_PATH, dev_num, tag);
	if (sysfs_read_float(sysfs_path, &current_freq) == -1) {
		snprintf(sysfs_path, PATH_MAX, DEVICE_SAMPLING_PATH, dev_num);
		if (sysfs_read_float(sysfs_path, &current_freq) == -1) {
			log_msg_and_exit_on_error(ERROR, "Can't read rate from: %s\n", sysfs_path); 
			set_test_state(FAILED);
			return -1;
		}    
	}

	if (current_freq == required_freq) {
		log_msg_and_exit_on_error(DEBUG, "%s: New data rate has the same value: %f\n",
			sysfs_path, required_freq);
		return 0;    
	}

	snprintf(sysfs_path, PATH_MAX, DEVICE_AVAIL_FREQ_PATH, dev_num);
	if (sysfs_read_str(sysfs_path, available_frequencies, BUFFER_SIZE) == -1) {
		log_msg_and_exit_on_error(ERROR, "Can't read from: %s\n", sysfs_path); 
		set_test_state(FAILED);
		return -1;
	}    

	cursor = available_frequencies;
	set_freq = -1;
	/* search closest value to set frequency from available frequencies */
	while (*cursor && cursor[0]) {
		old_freq = set_freq;
		set_freq = strtod(cursor, NULL);
		if (set_freq == max_freq)
			return set_freq;
		if (set_freq > max_freq) {
			return old_freq;
		}
		if (required_freq < set_freq) {
			return set_freq;
		}   
		if (fabs(required_freq - set_freq) <= 0.01) {
			return set_freq;
		}
		/* Skip digits */
		while (cursor[0] && !isspace(cursor[0]))
			cursor++;

		/* Skip spaces */
		while (cursor[0] && isspace(cursor[0]))
			cursor++;
	 
	}
	/* data rate is bigger than the biggest data rate available */
	return set_freq;
	
}

int write_freq(int sensor_index, float required_rate) {
	char sysfs_path[PATH_MAX];
	const char* tag;
	int dev_num;
	int enabled;
	float set_rate;
	float cur_hr_freq;
	int hr_trigger_nr;

	tag = g_sensor_info_iio_ext[sensor_index].tag;
	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	hr_trigger_nr = g_sensor_info_iio_ext[sensor_index].hr_trigger_nr;

	set_rate = select_closest_freq(sensor_index, required_rate);
	if (set_rate == -1) {
		return set_rate;
	}

	log_msg_and_exit_on_error(VERBOSE, "%s: setting data rate to %f\n", tag, set_rate);

	memset(sysfs_path, '\0', PATH_MAX);
	snprintf(sysfs_path, PATH_MAX, ENABLE_PATH, dev_num);
	if (sysfs_read_int(sysfs_path, &enabled) == -1) {
		log_msg_and_exit_on_error(ERROR, "Can't read value from %s\n", sysfs_path); 
		set_test_state(FAILED);
		return -1;
	}

	if (enabled && g_sensor_info_iio_ext[sensor_index].mode != MODE_POLL) {
		if (activate_sensor(sensor_index, 0) == -1)
			return -1;

	}
	
	if (hr_trigger_nr != -1) {
		snprintf(sysfs_path, PATH_MAX, TRIGGER_FREQ_PATH, hr_trigger_nr);
		if (sysfs_read_float(sysfs_path, &cur_hr_freq) == -1) {
			log_msg_and_exit_on_error(ERROR, "Can't read value from %s\n", sysfs_path); 
			set_test_state(FAILED);
			return -1;
		}
		if (cur_hr_freq != required_rate)
			if (sysfs_write_float(sysfs_path, required_rate) == -1) {
				log_msg_and_exit_on_error(ERROR, "Can't write value to %s\n", sysfs_path); 
				set_test_state(FAILED);
				return -1;
			}
	}
	/* new data rate is different from the old one */ 
	if (set_rate != 0) {
		snprintf(sysfs_path, PATH_MAX, SENSOR_SAMPLING_PATH, dev_num, tag);
		if (sysfs_write_float(sysfs_path, set_rate) == -1) {
			snprintf(sysfs_path, PATH_MAX, DEVICE_SAMPLING_PATH, dev_num);
			if (sysfs_write_float(sysfs_path, set_rate) == -1) {
				log_msg_and_exit_on_error(ERROR, "Can't write rate to: %s\n", sysfs_path); 
				set_test_state(FAILED);
				return -1;
			}    
		}
		
		g_sensor_info_iio_ext[sensor_index].data_rate = set_rate;
	}
	if (enabled && g_sensor_info_iio_ext[sensor_index].mode != MODE_POLL) {
		if (activate_sensor(sensor_index, 1) == -1)
			return -1;
	}
	if (set_rate != 0) {
		log_msg_and_exit_on_error(DEBUG, "%s: data rate set to %f\n", sysfs_path, set_rate);
	}
	return 0;
}

int set_freq(int sensor_index,  float required_value) {
	float set_value;
	float set_hr_value;
	int hr_trigger_nr;
	float new_value;
	int dev_num;
	const char* tag;
	char sysfs_path[PATH_MAX];
 
	memset(sysfs_path, '\0', PATH_MAX);
	hr_trigger_nr = g_sensor_info_iio_ext[sensor_index].hr_trigger_nr;
	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	tag = g_sensor_info_iio_ext[sensor_index].tag;
	if (write_freq(sensor_index,  required_value) == -1)
		return -1;
	
	new_value = select_closest_freq(sensor_index, required_value); 


	snprintf(sysfs_path, PATH_MAX, SENSOR_SAMPLING_PATH, dev_num, tag);
	if (sysfs_read_float(sysfs_path, &set_value) == -1) {
		snprintf(sysfs_path, PATH_MAX, DEVICE_SAMPLING_PATH, dev_num);
		if (sysfs_read_float(sysfs_path, &set_value) == -1) {
			log_msg_and_exit_on_error(ERROR, "%s: Cannot read!\n", sysfs_path);
			set_test_state(FAILED);
			return -1;
		}
	}
	/* set frequency for high rate trigger if any */
	if (hr_trigger_nr != -1) {
		snprintf(sysfs_path, PATH_MAX, TRIGGER_FREQ_PATH, hr_trigger_nr);
		if (sysfs_read_float( sysfs_path, &set_hr_value) == -1) {
			log_msg_and_exit_on_error(ERROR, "%s: Cannot read!\n", sysfs_path);
			set_test_state(FAILED);
			return -1;
		}
	}
	else
		set_hr_value = -1;

	   
	if ((set_value == new_value || new_value == 0) 
		&& (set_hr_value == required_value || set_hr_value == -1)) {
		log_msg_and_exit_on_error(VERBOSE, "Frequency for device %s was successfully"
			" set to value %f\n", g_sensor_info_iio_ext[sensor_index].tag, set_value);

		return 0;
	}
	else{
		log_msg_and_exit_on_error(ERROR, "Frequency for device %s was NOT set to value %f\n", 
			g_sensor_info_iio_ext[sensor_index].tag, required_value);
		set_test_state(FAILED);
		return -1;  
	}
}

int set_cdd_freq(int sensor_index) {
	float freq;

	freq = get_cdd_freq(sensor_index, 0);
	return set_freq(sensor_index,  freq);
}

bool set_freq_wrapper(void* key, void* value, void* context) {
	int sensor_index = (int)key;
	time_attributes_struct* time_attributes = (time_attributes_struct*)value;
	float freq = time_attributes->freq;
	set_freq(sensor_index, freq);    

	return true;
}
