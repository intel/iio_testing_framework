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
#include <stdint.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>
#include "cutils/hashmap.h"
#include "iio_control.h"
#include "iio_set_trigger.h"
#include "iio_sample_format.h"
#include "iio_utils.h"
#include "iio_common.h"

int enable_buffer(int sensor_index, int enabled) {
	char sysfs_path[PATH_MAX];
	int retries = ENABLE_BUFFER_RETRIES;
	int dev_num;

	memset(sysfs_path, '\0', PATH_MAX);
	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;


	snprintf(sysfs_path, PATH_MAX, ENABLE_PATH, dev_num);
	while (retries) {
		/* Low level, non-multiplexed, enable/disable routine */
		if (sysfs_write_int(sysfs_path, enabled) != -1) {
			log_msg_and_exit_on_error(VERBOSE, "Buffer is set to value: %d for device%d\n",
				enabled, dev_num);
			return 0;
		}

		log_msg_and_exit_on_error(DEBUG, "Failed enabling buffer on dev%d, retrying\n",
			dev_num);
		usleep(ENABLE_BUFFER_RETRY_DELAY_MS * 1000);
		retries--;
	}

	log_msg_and_exit_on_error(DEBUG, "Could not allocate buffer to value: %d for device%d\n",
		enabled, dev_num);
	return -1;
}

/* check if buffer is enabled */
int is_buffer_enable(int sensor_index, int value) {
	int dev_num;
	int set_value;
	char sysfs_path[PATH_MAX];

	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	memset(sysfs_path, '\0', PATH_MAX);

	snprintf(sysfs_path, PATH_MAX, ENABLE_PATH, dev_num);
	if(sysfs_read_int(sysfs_path, &set_value) == -1) {
		log_msg_and_exit_on_error(ERROR, "Can't read value from %s\n", sysfs_path); 
		set_test_state(FAILED);
		return -1;
	}

	if (set_value == value) {
		log_msg_and_exit_on_error(VERBOSE, "%s was successfully set on %d!\n",
			g_sensor_info_iio_ext[sensor_index].tag, value); 
	}
	else {
		log_msg_and_exit_on_error(ERROR, "%s was NOT set on %d!\n",
			g_sensor_info_iio_ext[sensor_index].tag, value);
		set_test_state(FAILED);
		return -1;
	}

	return 0;
}

int enable_all_buffers(int value) {
	int i;

	for (i = 0; i < g_sensor_info_size; i++) {
		if(g_sensor_info_iio_ext[i].discovered) {
			if(g_sensor_info_iio_ext[i].mode == MODE_POLL)
				continue;
			if (enable_buffer(i, value) == -1) {
				log_msg_and_exit_on_error(ERROR, "Can't enable buffer for %s!\n",
					g_sensor_info_iio_ext[i].tag); 
				set_test_state(FAILED);
				return -1;
			}
			if(is_buffer_enable(i, value) == -1)
				return -1;
		}
	}
	return 0;
}
/* disable all buffers */
int clean_up_sensors(void) {

	if(enable_all_buffers(0) == -1)
		return -1;
 
   return 0;   
}

int activate_sensor(int sensor_index, int value) {

	int set_value; 
	int dev_num;
	int i;
	char sysfs_path[PATH_MAX];

	memset(sysfs_path, '\0', PATH_MAX);
	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	
	if(g_sensor_info_iio_ext[sensor_index].mode == MODE_POLL) {
		log_msg_and_exit_on_error(ERROR, "Device%d is polling mode and "
			"can't be activated/deactivated!\n", dev_num);
		set_test_state(SKIPPED);
		return -1;
	}
	/* if sensor is already activated/deactivated fail */    
	snprintf(sysfs_path, PATH_MAX, ENABLE_PATH, dev_num);

	if(sysfs_read_int(sysfs_path, &set_value) == -1) {
		log_msg_and_exit_on_error(ERROR, "Can't read value from %s\n", sysfs_path); 
		set_test_state(FAILED);
		return -1;
	}
	if(set_value == value) {
		if(set_value) {
			log_msg_and_exit_on_error(ERROR, "%s was already activated!\n",
				g_sensor_info_iio_ext[sensor_index].tag); 
			set_test_state(FAILED);

		}
		else{
			log_msg_and_exit_on_error(ERROR, "%s was already deactivated!\n",
				g_sensor_info_iio_ext[sensor_index].tag); 
			set_test_state(FAILED);
		}

		return -1;

	}
	/* enable trigger if activate action*/
	if(value) {
		if (enable_trigger(dev_num, g_sensor_info_iio_ext[sensor_index].init_trigger_name) == -1) {
			log_msg_and_exit_on_error(ERROR, "Can't enable trigger for %s!\n",
				g_sensor_info_iio_ext[sensor_index].tag); 
			set_test_state(FAILED);
			return -1;
		}
	}
	if (enable_buffer(sensor_index, value) == -1) {
		log_msg_and_exit_on_error(ERROR, "Can't enable buffer for %s!\n",
			g_sensor_info_iio_ext[sensor_index].tag); 
		set_test_state(FAILED);
		return -1;
	}
	/* check if buffer was enabled/disabled properly */
	if(sysfs_read_int(sysfs_path, &set_value) == -1) {
		log_msg_and_exit_on_error(ERROR, "Can't read value from %s\n", sysfs_path); 
		set_test_state(FAILED);
		return -1;
	}

	if (set_value == value) {
		log_msg_and_exit_on_error(VERBOSE, "%s was successfully set on %d!\n",
			g_sensor_info_iio_ext[sensor_index].tag, value); 
	}
	else {
		log_msg_and_exit_on_error(ERROR, "%s was NOT set on %d!\n",
			g_sensor_info_iio_ext[sensor_index].tag, value);
		set_test_state(FAILED);
		return -1;
	}
	/* disable trigger if deactivate action */
	if(!value) {
		if(enable_trigger(dev_num, "\n") == -1) {
			log_msg_and_exit_on_error(ERROR, "Can't disable trigger for %s!\n",
				g_sensor_info_iio_ext[sensor_index].tag); 
			set_test_state(FAILED);
			return -1;
		}
	}
	return 0;   
}
/* convert to syntax necessary for hashmap library */
bool activate_sensor_wrapper(void* key, void* value, void* context) {
	int sensor_index = (int)key;
	int activate_value = (int)context;
	activate_sensor(sensor_index, activate_value);    

	return true;
}

int activate_deactivate_sensor(int sensor_index, int counter) {
	int i;
	if(counter <= 0) {
		log_msg_and_exit_on_error(ERROR, "Wrong value for counter!\n");
		set_test_state(FAILED);
		return -1;
	}
	for (i = 0; i < counter; ++i) {
		if(activate_sensor(sensor_index, 1) == -1)
			return -1;
		if(activate_sensor(sensor_index, 0) == -1)
			return -1;
		
	}
	return 0;
}
/* convert to syntax necessary for hashmap library */
bool activate_deactivate_sensor_wrapper(void* key, void* value, void* context) {
	int sensor_index = (int)key;
	int counter = (int)context;
	activate_deactivate_sensor(sensor_index, counter);    

	return true;
}

int activate_all_sensors(int value) {
	int sensor;

	for (sensor = 0; sensor < g_sensor_info_size; ++sensor) {
		if(g_sensor_info_iio_ext[sensor].discovered) {  
			if(g_sensor_info_iio_ext[sensor].mode == MODE_POLL)
				continue;
			if(activate_sensor(sensor, value) == -1)
				return -1;
		}
	}
	return 0;   
}

int activate_deactivate_all_sensors(int counter) {
	int i;

	if(counter <= 0) {
		log_msg_and_exit_on_error(ERROR, "Wrong value for counter!\n");
		set_test_state(FAILED);
		return -1;
	}
	for (i = 0; i < counter; ++i) {
		if(activate_all_sensors(1) == -1)
			return -1;
		if(activate_all_sensors(0) == -1)
			return -1;

	}
	return 0;
}

int get_index_from_dev_num(int dev_num) {
	if (dev_num < 0)
		return -1;
	
	int s;
	
	for (s = 0; s < g_sensor_info_size; s++) {
		if (!g_sensor_info_iio_ext[s].discovered)
			continue;
		
		if(g_sensor_info_iio_ext[s].dev_num == dev_num)
			return s;        
	}
	return -1;
}

int get_index_from_tag(char *tag) {
	if(tag == NULL)
		return -1;

	int s;

	for (s = 0; s < g_sensor_info_size; s++) {
		if (!g_sensor_info_iio_ext[s].discovered)
			continue;
		if(strncmp(g_sensor_info_iio_ext[s].tag, tag, strlen(tag)) == 0) {
		   return s;         
		}
	}
	return -1;
}

/* read channels values and timestamp for polling mode sensors */
int get_data_polling_mode(int sensor_index) {
	int num_channels;
	int c;
	int dev_num;
	int value;
	float scaled_value;
	char sysfs_path[PATH_MAX];
	const char* tag;
	const char* name;

	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	num_channels = g_sensor_info_iio_ext[sensor_index].num_channels;
	tag = g_sensor_info_iio_ext[sensor_index].tag;
	memset(sysfs_path, '\0', PATH_MAX);
	for (c = 0; c < num_channels; ++c) {
		name = g_sensor_info_iio_ext[sensor_index].channel_descriptor[c].name;
		
		snprintf(sysfs_path, PATH_MAX, BASE_PATH "%s", dev_num, 
			g_sensor_info_iio_ext[sensor_index].channel_descriptor[c].raw_path);
		
		if (sysfs_read_int(sysfs_path, &value) == -1) {
			snprintf(sysfs_path, PATH_MAX, BASE_PATH "%s", dev_num, 
			g_sensor_info_iio_ext[sensor_index].channel_descriptor[c].input_path);

			if (sysfs_read_int(sysfs_path, &value) == -1) {
				log_msg_and_exit_on_error(ERROR, "Can't read values from [%s] or [%s]\n", 
					g_sensor_info_iio_ext[sensor_index].channel_descriptor[c].raw_path, 
					g_sensor_info_iio_ext[sensor_index].channel_descriptor[c].input_path);
				set_test_state(FAILED);
				return -1;
			}    
		}
		log_msg_and_exit_on_error(VERBOSE, "Device %s has on channel %s value = %d\n",
			g_sensor_info_iio_ext[sensor_index].tag, name, value);
		scaled_value = scale_value(sensor_index, c, value);
		log_msg_and_exit_on_error(VERBOSE, "Device %s has on channel %s scaled value = %f\n",
			g_sensor_info_iio_ext[sensor_index].tag, name, scaled_value);
		g_sensor_info_iio_ext[sensor_index].channel_info[c].last_value = scaled_value;
	}
	return 0;
}

/* read channels values and timestamp for triggered mode sensors */
int get_data_triggered_mode(int sensor_index) {
	char buf[g_sensor_info_iio_ext[sensor_index].sample_size];
	int num_channels;
	int c;
	int counter;
	int index;
	int padding;
	int timestamp_index;
	int return_value;
	int fd;
	int64_t last_timestamp;
	int64_t value;
	float new_value;
	channel_info_t channel;
	channel_info_t timestamp;
	unsigned char* sample;

	counter = 0;
	timestamp = g_sensor_info_iio_ext[sensor_index].timestamp;
	timestamp_index = timestamp.index;
	num_channels = g_sensor_info_iio_ext[sensor_index].num_channels;
	fd = g_sensor_info_iio_ext[sensor_index].read_fd;
	
	if (sysfs_read_from_fd(fd, buf, g_sensor_info_iio_ext[sensor_index].sample_size) == -1) {
		log_msg_and_exit_on_error(ERROR, "Can't read samples from %s \n",
			g_sensor_info_iio_ext[sensor_index].tag); 
		set_test_state(FAILED);
		return -1;
	}
	for (index = 0; index < (num_channels + 1); ++index) {     
		if (index == timestamp_index) {
			sample = (unsigned char*)malloc(timestamp.size);
			if (sample == NULL) {   
				log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
				exit(-1);
			}
			/* check if there is any padding */
			padding = get_padding_size(counter, timestamp.type_info.storagebits);
			counter += padding; 
			memcpy(sample, buf + counter, timestamp.size);
			value = sample_as_int64(sample, &timestamp.type_info);
			free(sample);
			counter += timestamp.size;
			last_timestamp = g_sensor_info_iio_ext[sensor_index].last_timestamp;
			log_msg_and_exit_on_error(VERBOSE, "Device %s has last timestamp %lld\n",
				g_sensor_info_iio_ext[sensor_index].tag, last_timestamp);
			log_msg_and_exit_on_error(VERBOSE, "Device %s has new  timestamp %lld\n",
				g_sensor_info_iio_ext[sensor_index].tag, value);
			g_sensor_info_iio_ext[sensor_index].last_timestamp = value;
		}
		else
			for (c = 0; c < num_channels; c++) {
				channel = g_sensor_info_iio_ext[sensor_index].channel_info[c];
				if(index == channel.index) {  
					sample = (unsigned char*)malloc(channel.size);
					if (sample == NULL) {   
						log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
						exit(-1);
					}
					/* check if there is any padding */
					padding = get_padding_size(counter, channel.type_info.storagebits);
					counter += padding; 
					memcpy(sample, buf + counter, channel.size);
					value = sample_as_int64(sample, &channel.type_info);
					free(sample);
					/* scale value */
					new_value = scale_value(sensor_index, c, value); 
					counter += channel.size;
					g_sensor_info_iio_ext[sensor_index].channel_info[c].last_value = new_value;
					log_msg_and_exit_on_error(VERBOSE, "Device %s has scaled value for %s is %f\n", 
						g_sensor_info_iio_ext[sensor_index].tag,
						g_sensor_info_iio_ext[sensor_index].channel_descriptor[c].name, new_value);
					log_msg_and_exit_on_error(VERBOSE, "Device %s has value for %s is %lld\n",
						g_sensor_info_iio_ext[sensor_index].tag,
						g_sensor_info_iio_ext[sensor_index].channel_descriptor[c].name, value);
				}
			}        
	} 
	return 0;   
}


void list_sensors(void) {
	int s;

	for (s = 0; s < g_sensor_info_size; ++s) {
		if(g_sensor_info_iio_ext[s].discovered) {
			if(g_sensor_info_iio_ext[s].mode == MODE_POLL)
				log_msg_and_exit_on_error(DEBUG, "Found device %s in polling mode\n",
					g_sensor_info_iio_ext[s].tag);
			else if(g_sensor_info_iio_ext[s].mode == MODE_TRIGGER)
				log_msg_and_exit_on_error(DEBUG, "Found device %s in triggered mode\n",
					g_sensor_info_iio_ext[s].tag);
		}
	}
	log_msg_and_exit_on_error(DEBUG, "Found %d devices!\n", g_sensor_iio_count);
}


/* check if channels are configured for every discovered sensor */
int check_channels(int sensor_index) {
	int channel;
	int num_channels;
	int dev_num;
	char mapped[g_sensor_info_iio_ext[sensor_index].num_channels];
	char sysfs_dir[PATH_MAX];
	DIR *dir;
	struct dirent *d;
	
	num_channels = g_sensor_info_iio_ext[sensor_index].num_channels;
	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	memset(mapped, 0, num_channels);
	memset(sysfs_dir, '\0', PATH_MAX);
	
	/* for triggered devices */
	if(g_sensor_info_iio_ext[sensor_index].mode == MODE_TRIGGER) {

		snprintf(sysfs_dir, PATH_MAX, CHANNEL_PATH, dev_num);

		dir = opendir(sysfs_dir);
		if (!dir) {
			log_msg_and_exit_on_error(ERROR, "(%s): Can't open: %s", strerror(errno), sysfs_dir);
			set_test_state(FAILED);
			return -1;
		}
		while ((d = readdir(dir))) {
			if (!strncmp(d->d_name, ".", 1) || !strncmp(d->d_name, "..", 2))
				continue;
			for (channel = 0; channel < num_channels; ++channel) {
				if (g_sensor_info_iio_ext[sensor_index].channel_descriptor[channel].en_path &&
						!strcmp(d->d_name, g_sensor_info_iio_ext[sensor_index].channel_descriptor[channel].en_path)) {
					
					log_msg_and_exit_on_error(DEBUG, "Found channel %s for device %s!\n", 
						g_sensor_info_iio_ext[sensor_index].channel_descriptor[channel].name, 
						g_sensor_info_iio_ext[sensor_index].tag);
					mapped[channel] = 1;
					break;
				}
			}
		}
	}
	/* for polling devices */ 
	else if(g_sensor_info_iio_ext[sensor_index].mode == MODE_POLL) {
		snprintf(sysfs_dir, PATH_MAX, BASE_PATH, dev_num);
		dir = opendir(sysfs_dir);
		
		if (!dir) {
			log_msg_and_exit_on_error(ERROR, "(%s): Can't open: %s", strerror(errno), sysfs_dir);
			set_test_state(FAILED);
			return -1;
		}

		while ((d = readdir(dir))) {
			if (!strncmp(d->d_name, ".", 1) || !strncmp(d->d_name, "..", 2))
				continue;
			
			for (channel = 0; channel < num_channels; ++channel) {
				if (!strcmp(d->d_name, g_sensor_info_iio_ext[sensor_index].channel_descriptor[channel].raw_path) ||
					!strcmp(d->d_name, g_sensor_info_iio_ext[sensor_index].channel_descriptor[channel].input_path)) {    
					log_msg_and_exit_on_error(DEBUG, "Found channel %s for device %s!\n", 
						g_sensor_info_iio_ext[sensor_index].channel_descriptor[channel].name, 
						g_sensor_info_iio_ext[sensor_index].tag);
					mapped[channel] = 1;
					break;    
				}
			}
		}
	}
		
	for (channel = 0; channel < num_channels; ++channel) {
		if(!mapped[channel]) {
			log_msg_and_exit_on_error(ERROR, "Device %s doesn't have channel %s configured!", 
				g_sensor_info_iio_ext[sensor_index].tag,
				g_sensor_info_iio_ext[sensor_index].channel_descriptor[channel].name);
			set_test_state(FAILED);
			return -1;
		}
	}

	return 0;
}

/* convert to syntax necessary for hashmap library */
bool check_channels_wrapper(void* key, void* value, void* context) {
	int sensor_index = (int)key;
	check_channels(sensor_index);    
	return true;
}
