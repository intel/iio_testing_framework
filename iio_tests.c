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
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/stat.h> 
#include <fcntl.h> 
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>
#include <pthread.h>
#include "cutils/hashmap.h"
#include "iio_tests.h"
#include "iio_control.h"
#include "iio_control_frequency.h"
#include "iio_utils.h"

Hashmap *map_fd_to_sensor_index;
static int epfd;
static pthread_t threads[MAX_SENSORS];

/* collect and compute data necessary to measure frequency for each sensor */ 
int measure_freq_wrapper(int sensor_index, void* timestamp_info_param, int stage) {
	int64_t last_timestamp;
	int64_t new_timestamp;
	int64_t delay;    
	int nr;
	int buf_size;
	float measured_rate;
	float set_rate;
	buf_size = g_sensor_info_iio_ext[sensor_index].sample_size;
	char buf[buf_size];
	timestamp_info_struct *timestamp_info;
	
	memset(buf, '\0', buf_size);
	
	/* collect data from sensors */
	if (stage == PROCESS) {
		last_timestamp = g_sensor_info_iio_ext[sensor_index].last_timestamp;
		new_timestamp = get_timestamp_realtime();
		g_sensor_info_iio_ext[sensor_index].last_timestamp = new_timestamp;

		if (sysfs_read_from_fd(g_sensor_info_iio_ext[sensor_index].read_fd, buf, buf_size) == -1) {
			log_msg_and_exit_on_error(ERROR, "Can't read samples from %s \n",
				g_sensor_info_iio_ext[sensor_index].tag); 
			set_test_state(FAILED);
			return -1;
		}
		/* don't compute any difference for first value */
		if (last_timestamp != -1) {
			timestamp_info = (timestamp_info_struct*)timestamp_info_param;
			timestamp_info->all_consec_timestamps_diff += (new_timestamp - last_timestamp);
			timestamp_info->counter ++;
			
			log_msg_and_exit_on_error(VERBOSE, "Device %s  has system last timestamp %lld ns\n",
				g_sensor_info_iio_ext[sensor_index].tag, last_timestamp);
			log_msg_and_exit_on_error(VERBOSE, "Device %s  has system new timestamp  %lld ns\n",
				g_sensor_info_iio_ext[sensor_index].tag, new_timestamp);        
			log_msg_and_exit_on_error(VERBOSE, "Device %s  has difference between system timestamps %lld ns\n",
				g_sensor_info_iio_ext[sensor_index].tag, 
				new_timestamp - last_timestamp);
		}
	}
	/* compute collected data */
	else{
		timestamp_info = (timestamp_info_struct*)timestamp_info_param;
		nr = timestamp_info->counter;
		delay = timestamp_info->all_consec_timestamps_diff; 
		free(timestamp_info);  
		if (nr == 0) {
			log_msg_and_exit_on_error(ERROR, "No data received from %s\n", g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
			return -1;
		}

		log_msg_and_exit_on_error(DEBUG, "Device %s  has frequency %f\n", g_sensor_info_iio_ext[sensor_index].tag, 
			g_sensor_info_iio_ext[sensor_index].data_rate);
		
		delay /= nr;
		measured_rate = (CONVERT_SEC_TO_NANO(1)/delay);
		set_rate = g_sensor_info_iio_ext[sensor_index].data_rate;

		/* measured rate rate is OK
		** the absolute difference between measured rate and set rate is 
		** less than 10% from set rate;
		** this variation was empirical established
		*/
		if (fabs(measured_rate - set_rate) <= set_rate/10) {
			log_msg_and_exit_on_error(DEBUG, "Rate measured for device %s = %f is close to set rate = %f\n", 
				g_sensor_info_iio_ext[sensor_index].tag, measured_rate, set_rate);
			return 0;
		}
		/* there is a big difference between measured rate and set rate */
		log_msg_and_exit_on_error(ERROR, "Rate measured for device %s = %f is different than set rate = %f\n", 
			g_sensor_info_iio_ext[sensor_index].tag, measured_rate, set_rate);
		set_test_state(FAILED);
		return -1;  

	   
	}
	return 0;
}
/* check if difference between every client delay and set delay
** is less than test given delay
*/
int check_sample_timestamp_difference_wrapper(int sensor_index, void* timestamp_info_param, int stage) {
	int set_delay;
	int difference_delay;
	int nr;
	int64_t last_timestamp;
	int64_t new_timestamp;
	int delay;
	int max_delay;
	float new_freq;
	timestamp_info_struct *timestamp_info;
	time_attributes_struct* time_attributes;

	/* collect data from sensors */
	if (stage == PROCESS) {
		last_timestamp = g_sensor_info_iio_ext[sensor_index].last_timestamp;
		if (get_data_triggered_mode(sensor_index) == -1)
			return -1; 
		new_timestamp = g_sensor_info_iio_ext[sensor_index].last_timestamp;
		/* don't compute any difference for first value */
		if (last_timestamp != -1) {
			
			timestamp_info = (timestamp_info_struct*)timestamp_info_param;
			time_attributes = (time_attributes_struct*)hashmapGet(map_sensor_index_to_time_attributes,
				(void*)sensor_index);
			max_delay = time_attributes->max_delay;
			
			delay = CONVERT_NANO_TO_MILLI(new_timestamp - last_timestamp);
			set_delay = CONVERT_SEC_TO_MILLI(1/g_sensor_info_iio_ext[sensor_index].data_rate);
			
			difference_delay = abs(delay - set_delay);

			if (difference_delay > max_delay) {
				log_msg_and_exit_on_error(ERROR, "Device %s exceed max sample timestamp difference = %d ms, "
					"having %d ms delay\n", 
					g_sensor_info_iio_ext[sensor_index].tag, max_delay, difference_delay);
				set_test_state(FAILED);
				if (timestamp_info->counter != -1)
					timestamp_info->counter = -1;
			}
			/*if there weren't any errors, keep counting to know there have been received samples */
			else{
				log_msg_and_exit_on_error(DEBUG, "Measured sample timestamp difference is %d ms "
					"less than max sample timestamp difference = %d ms\n",
					difference_delay, max_delay);
				if (timestamp_info->counter != -1)
					timestamp_info->counter ++;
			}

		}
		return 0;
	}

	/* compute collected data */
	else {
		timestamp_info = (timestamp_info_struct*)timestamp_info_param;
		time_attributes = (time_attributes_struct*)hashmapGet(map_sensor_index_to_time_attributes,
			(void*)sensor_index);
		max_delay = time_attributes->max_delay;
		nr = timestamp_info->counter;
		free(timestamp_info);
		if (nr == 0) {
			log_msg_and_exit_on_error(ERROR, "No data received from %s\n",
				g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
			return -1;
		}  

		log_msg_and_exit_on_error(DEBUG, "Device %s  has frequency %f\n",
			g_sensor_info_iio_ext[sensor_index].tag, 
			g_sensor_info_iio_ext[sensor_index].data_rate);

		if (nr == -1) {
			log_msg_and_exit_on_error(ERROR, "Device %s exceed max sample timestamp difference = %d ms\n", 
				g_sensor_info_iio_ext[sensor_index].tag, max_delay);
			set_test_state(FAILED);
			return -1;
		}
		log_msg_and_exit_on_error(DEBUG, "Device %s hasn't exceed max sample timestamp difference = %d ms\n",
			g_sensor_info_iio_ext[sensor_index].tag, max_delay);
		return 0;    
	}

}
/* check if difference between medium client delay and set delay
** is less than test given delay
*/
int check_sample_timestamp_average_difference_wrapper(int sensor_index,
	void *timestamp_info_param, int stage) {

	int nr;
	int set_delay;
	int avg_delay;
	int max_delay;
	int difference_delay;
	int64_t last_timestamp;
	int64_t new_timestamp;
	int64_t delay;
	timestamp_info_struct *timestamp_info;
	time_attributes_struct* time_attributes;

	/* collect data from sensors */
	if (stage == PROCESS) {
		last_timestamp = g_sensor_info_iio_ext[sensor_index].last_timestamp;
		if (get_data_triggered_mode(sensor_index) == -1)
			return -1; 
		new_timestamp = g_sensor_info_iio_ext[sensor_index].last_timestamp;
		/* don't compute any difference for first value */
		if (last_timestamp != -1) {
			timestamp_info = (timestamp_info_struct*)timestamp_info_param;
			timestamp_info->all_consec_timestamps_diff += (new_timestamp - last_timestamp);
			timestamp_info->counter ++;
			log_msg_and_exit_on_error(VERBOSE, "Device %s  has difference between sample timestamp %d ms\n",
				g_sensor_info_iio_ext[sensor_index].tag, CONVERT_NANO_TO_MILLI(new_timestamp - last_timestamp));
		}
	}

	/* compute collected data */
	else{
		timestamp_info = (timestamp_info_struct*)timestamp_info_param;
		delay = timestamp_info->all_consec_timestamps_diff;
		time_attributes = (time_attributes_struct*)hashmapGet(map_sensor_index_to_time_attributes, (void*)sensor_index);
		max_delay = time_attributes->max_delay;
		nr = timestamp_info->counter;
		free(timestamp_info);
		if (nr == 0) {
			log_msg_and_exit_on_error(ERROR, "No data received from %s\n", g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
			return -1;
		}

		log_msg_and_exit_on_error(DEBUG, "Device %s  has frequency %f\n", g_sensor_info_iio_ext[sensor_index].tag, 
			g_sensor_info_iio_ext[sensor_index].data_rate);

		avg_delay = (float)(CONVERT_NANO_TO_MILLI(delay)/nr); 
		set_delay = CONVERT_SEC_TO_MILLI(1/g_sensor_info_iio_ext[sensor_index].data_rate);
		
		difference_delay = abs(avg_delay - set_delay);

		if (difference_delay > max_delay) {
			log_msg_and_exit_on_error(ERROR, "Device %s exceed max sample timestamp average difference = %d ms, having %d ms delay\n", 
				g_sensor_info_iio_ext[sensor_index].tag, max_delay, difference_delay);
			set_test_state(FAILED);
			return -1;
		}
		else{
			log_msg_and_exit_on_error(DEBUG, "Device %s has measured sample timestamp average difference = %d ms less than" 
				" max sample timestamp average difference = %d ms\n", g_sensor_info_iio_ext[sensor_index].tag, 
					difference_delay, max_delay);
		}
		
	}
	return 0;    
}
/* check if each difference between system timestamp and client
** timestamp is less than test given delay
*/
int check_client_delay_wrapper(int sensor_index, void* timestamp_info_param, int stage) {
	int nr;
	int64_t sample_timestamp;
	int64_t sys_timestamp;
	int delay;
	int max_delay;
	timestamp_info_struct *timestamp_info;
	time_attributes_struct* time_attributes;

	/* collect data from sensors */
	if (stage == PROCESS) {
		if (get_data_triggered_mode(sensor_index) == -1)
			return -1; 
		sample_timestamp = g_sensor_info_iio_ext[sensor_index].last_timestamp;
		sys_timestamp = get_timestamp_realtime();

		log_msg_and_exit_on_error(VERBOSE, "Value for system timestamp is %lld: \n", sys_timestamp);
		
		timestamp_info = (timestamp_info_struct*)timestamp_info_param;
		time_attributes = (time_attributes_struct*)hashmapGet(map_sensor_index_to_time_attributes,
			(void*)sensor_index);
		max_delay = time_attributes->max_delay;
		
		delay = abs(CONVERT_NANO_TO_MILLI(sample_timestamp - sys_timestamp));
		
		if (delay > max_delay) {
			log_msg_and_exit_on_error(ERROR, "Device %s exceed max client delay = %d ms, having %d ms delay\n", 
				g_sensor_info_iio_ext[sensor_index].tag, max_delay, delay);
			set_test_state(FAILED);
			if (timestamp_info->counter != -1) {
				timestamp_info->counter = -1;
			}
		}    
		/*if there weren't any errors, keep counting to know there have been received samples */
		else{
			log_msg_and_exit_on_error(DEBUG, "Device %s has measured client delay = %d ms less "
				"than max client delay = %d ms\n",
				g_sensor_info_iio_ext[sensor_index].tag, delay, max_delay);
			if (timestamp_info->counter != -1) {
				timestamp_info->counter ++;
			}
		}
		return 0;

	}

	/* compute collected data */
	else{
		timestamp_info_struct *timestamp_info = (timestamp_info_struct*)timestamp_info_param;
		time_attributes = (time_attributes_struct*)hashmapGet(map_sensor_index_to_time_attributes,
			(void*)sensor_index);
		max_delay = time_attributes->max_delay;
		nr = timestamp_info->counter;
		free(timestamp_info);
		if (nr == 0) {
			log_msg_and_exit_on_error(ERROR, "No data received from %s\n",
				g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
			return -1;
		}

		log_msg_and_exit_on_error(DEBUG, "Device %s  has frequency %f\n",
			g_sensor_info_iio_ext[sensor_index].tag, g_sensor_info_iio_ext[sensor_index].data_rate);

		if (nr == -1) {
			log_msg_and_exit_on_error(ERROR, "Device %s exceed max client delay = %d ms\n", 
				g_sensor_info_iio_ext[sensor_index].tag, max_delay);
			return -1;
		}
		log_msg_and_exit_on_error(DEBUG, "Device %s hasn't exceed max client delay = %d ms\n",
			g_sensor_info_iio_ext[sensor_index].tag, max_delay);
		return 0;    
		
	}

}
/* check if medium difference between system timestamp and client
** timestamp is less than test given delay
*/
int check_client_average_delay_wrapper(int sensor_index,
	void* timestamp_info_param, int stage) {

	int nr;
	int max_delay;
	int64_t sys_timestamp;
	int64_t sample_timestamp;
	int64_t delay;
	int avg_delay;
	timestamp_info_struct *timestamp_info;
	time_attributes_struct* time_attributes;

	/* collect data from sensors */
	if (stage == PROCESS) {
		if (get_data_triggered_mode(sensor_index) == -1)
			return -1; 
		sample_timestamp = g_sensor_info_iio_ext[sensor_index].last_timestamp;
		sys_timestamp = get_timestamp_realtime();
		
		timestamp_info = (timestamp_info_struct*)timestamp_info_param;
		timestamp_info->all_consec_timestamps_diff += llabs(sample_timestamp - sys_timestamp);
		timestamp_info->counter ++;
		
		log_msg_and_exit_on_error(VERBOSE, "Device %s  has system timestamp  %lld ns\n",
			g_sensor_info_iio_ext[sensor_index].tag, sys_timestamp);
		log_msg_and_exit_on_error(VERBOSE, "Device %s  has client delay %lld ns\n",
			g_sensor_info_iio_ext[sensor_index].tag, llabs(sys_timestamp - sample_timestamp));
	}

	/* compute collected data */
	else{
		timestamp_info = (timestamp_info_struct*)timestamp_info_param;
		delay = timestamp_info->all_consec_timestamps_diff;
		time_attributes = (time_attributes_struct*)hashmapGet(map_sensor_index_to_time_attributes,
			(void*)sensor_index);
		max_delay = time_attributes->max_delay;
		nr = timestamp_info->counter;
		free(timestamp_info);
		if (nr == 0) {
			log_msg_and_exit_on_error(ERROR, "No data received from %s\n",
				g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
			return -1;
		}

		log_msg_and_exit_on_error(DEBUG, "Device %s  has frequency %f\n",
			g_sensor_info_iio_ext[sensor_index].tag, 
			g_sensor_info_iio_ext[sensor_index].data_rate);


		avg_delay = (float)(CONVERT_NANO_TO_MILLI(delay)/nr); 
		
		if (avg_delay > max_delay) {
			log_msg_and_exit_on_error(ERROR, "Device %s exceed max client average delay = %d ms, "
				"having %d ms delay\n", g_sensor_info_iio_ext[sensor_index].tag, max_delay, avg_delay);
			set_test_state(FAILED);
			return -1;
		}
		else{
			log_msg_and_exit_on_error(DEBUG, "Measured client average delay is %d ms less "
				"than max client average delay = %d ms\n", avg_delay, max_delay);
		}
	}
	return 0;
}
/* collect and compute data necessary to measure standard deviation for each sensor */ 
int standard_deviation_wrapper(int sensor_index,
	void *standard_deviation_info_param, int stage) {

	int dev_num;
	int counter; 
	int redundant_part;
	int init;
	int final; 
	int i;
	int channel;  
	int num_channels;
	int channels_values_size;
	int error;
	float standard_devs[g_sensor_info_iio_ext[sensor_index].num_channels];
	standard_deviation_struct* standard_deviation_info;
	char buffer[BUFFER_SIZE];
	float medium; 
	float standard_dev;
	float max_dev;
	const char* name;

	num_channels = g_sensor_info_iio_ext[sensor_index].num_channels;
	error = 0;
	memset(buffer, '\0', BUFFER_SIZE);
	
	/* collect data from sensors */
	if (stage == PROCESS) {
		standard_deviation_info = (standard_deviation_struct*)standard_deviation_info_param;
		counter = standard_deviation_info->counter;
		channels_values_size = standard_deviation_info->channels_values_size;
		 
		if (counter >= channels_values_size) {
			standard_deviation_info->channels_values_size *= 2;
			for (i = 0; i < num_channels; i++) {
				standard_deviation_info->channels_values[i] =
					(float*)realloc(standard_deviation_info->channels_values[i],
					channels_values_size * sizeof(float));
					
				if (standard_deviation_info->channels_values[i] == NULL) {
					log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
					set_test_state(FAILED);
					exit(-1);
				}
			}
		}
		if (g_sensor_info_iio_ext[sensor_index].mode == MODE_TRIGGER) {
			if (get_data_triggered_mode(sensor_index) == -1) {
				return -1;
			}
		}    
		else{ 
			if (get_data_polling_mode(sensor_index) == -1) {
				return -1;
			}
			if (sysfs_read_from_fd(g_sensor_info_iio_ext[sensor_index].read_fd, buffer, 1) == -1) {
				log_msg_and_exit_on_error(ERROR, "Can't read samples from %s \n",
					g_sensor_info_iio_ext[sensor_index].tag); 
				set_test_state(FAILED);
				return -1;
			}
		}       
		for (channel = 0; channel < num_channels; ++channel) {
			standard_deviation_info->channels_values[channel][counter] =
				g_sensor_info_iio_ext[sensor_index].channel_info[channel].last_value;
		}
		standard_deviation_info->counter++;

	/*compute standard deviation value */
	} else {

		if (g_sensor_info_iio_ext[sensor_index].mode == MODE_POLL) {
			if (pthread_join(threads[sensor_index], NULL)) {
				log_msg_and_exit_on_error(ERROR, "Can't destroy thread for sensor %s\n",
					g_sensor_info_iio_ext[sensor_index].tag);
				set_test_state(FAILED);
				return -1;
			}
		}
		
		standard_deviation_info = (standard_deviation_struct*)standard_deviation_info_param;
		counter = standard_deviation_info->counter;
			
		max_dev = get_standard_deviation_value(sensor_index);
		if (counter == 0) {
			log_msg_and_exit_on_error(ERROR, "No data received from %s\n",
				g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
			return -1;
		}
		log_msg_and_exit_on_error(DEBUG, "Got %d measurements from %s\n", counter,
			g_sensor_info_iio_ext[sensor_index].tag);
		/* compute standard deviation for each channel */            

		for (channel = 0; channel < num_channels; ++channel) {
			/* first 10% samples are considered to be redundant */
			
			redundant_part = (int)counter/10;
			init = redundant_part;
			final = counter;
			counter -= redundant_part;
			

			standard_dev = 0.0;
			medium = 0.0;
			for (i = init; i < final; ++i) {
				medium += standard_deviation_info->channels_values[channel][i];
			}
			medium /= counter;
			log_msg_and_exit_on_error(DEBUG, "Device %s has medium value on channel %s = %f\n",
				g_sensor_info_iio_ext[sensor_index].tag, 
				g_sensor_info_iio_ext[sensor_index].channel_descriptor[channel].name, medium);
			for (i = init; i < final; ++i) {
				standard_dev +=
					(standard_deviation_info->channels_values[channel][i] - medium) *
					(standard_deviation_info->channels_values[channel][i] - medium);
			} 

			standard_dev /= counter;
			standard_devs[channel] = sqrt(standard_dev);
		}

		for (i = 0; i < num_channels; i++) {
			free(standard_deviation_info->channels_values[i]);
		}
		free(standard_deviation_info->channels_values);
		free(standard_deviation_info);
		/* check max deviation */
		for (channel = 0; channel < num_channels; ++channel) {
			name = g_sensor_info_iio_ext[sensor_index].channel_descriptor[channel].name;
			if (standard_devs[channel] > max_dev) {
				log_msg_and_exit_on_error(ERROR, "Deviation measured for device %s for channel %s = %f "
					"is bigger than real standard deviation = %f\n", g_sensor_info_iio_ext[sensor_index].tag,
					name, standard_devs[channel], max_dev);
				set_test_state(FAILED);
				error = 1;
			}
			else{
				log_msg_and_exit_on_error(DEBUG, "Deviation measured for device %s on channel %s is %f "
					"close to real standard deviation = %f\n", g_sensor_info_iio_ext[sensor_index].tag,
					name, standard_devs[channel], max_dev);
			}
		}
		if (error)
			return -1;
	}
	return 0;
}
/* compute and process data necessary to measure jitter for each sensor */ 
int test_jitter_wrapper(int sensor_index, void* jitter_info_param, int stage) {
	int counter;
	int timestamp_values_size;  
	int i;  
	int64_t *timestamp;
	int64_t *diff_timestamp;
	float medium;
	float standard_dev;
	jitter_struct* jitter_info;

	standard_dev = 0.0;
	medium = 0.0;
	
	/* collect data from sensors */
	if (stage == PROCESS) {
		jitter_info = (jitter_struct*)jitter_info_param;
		counter = jitter_info->counter;
		timestamp_values_size = jitter_info->timestamp_values_size;
		if (counter >= timestamp_values_size) {
			timestamp_values_size *= 2;
			jitter_info->timestamp_values =
				(int64_t *)realloc(jitter_info->timestamp_values,
				timestamp_values_size * sizeof(int64_t));

			if (jitter_info->timestamp_values == NULL) {
				log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
				set_test_state(FAILED);
				exit(-1);
			}
			jitter_info->timestamp_values_size = timestamp_values_size;
		}

		if (get_data_triggered_mode(sensor_index) == -1)
			return -1;
		jitter_info->timestamp_values[counter] =
			g_sensor_info_iio_ext[sensor_index].last_timestamp;

		jitter_info->counter ++;            
	}

	/* compute value of jitter */
	else{
		jitter_info = (jitter_struct*)jitter_info_param;
		counter = jitter_info->counter;

		if (counter == 0) {
			log_msg_and_exit_on_error(ERROR, "No data received from %s\n",
				g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
			free(jitter_info->timestamp_values);
			free(jitter_info);
			return -1;
		}
		/* compute jitter */
		counter --;
		diff_timestamp = (int64_t *)malloc(counter * sizeof(int64_t));
		if (diff_timestamp == NULL) {
			log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
			set_test_state(FAILED);
			exit(-1);
		}

		for (i = 0; i < counter; ++i) {
			diff_timestamp[i] =
				jitter_info->timestamp_values[i + 1] - jitter_info->timestamp_values[i];
		}
		free(jitter_info->timestamp_values);
		free(jitter_info);
		for (i = 0; i < counter; ++i) {
			medium += diff_timestamp[i];
		}
		medium /= counter;
		for (i = 0; i < counter; ++i) {
			standard_dev += (diff_timestamp[i] - medium) * (diff_timestamp[i] - medium);
		} 

		free(diff_timestamp);
		
		standard_dev /= counter;
		standard_dev = sqrt(standard_dev);

		log_msg_and_exit_on_error(DEBUG, "Device %s has standard deviation for difference "
			"between sample timestamps = %f\n", g_sensor_info_iio_ext[sensor_index].tag,
			standard_dev);
		log_msg_and_exit_on_error(DEBUG, "Device %s has medium difference between "
			"sample timestamps = %f\n", g_sensor_info_iio_ext[sensor_index].tag, medium);

		standard_dev = standard_dev / medium * 100;
		
		/* check jitter */
		if (standard_dev > (float)MAX_JITTER) {
			log_msg_and_exit_on_error(ERROR, "Jitter measured for device %s = %f is bigger "
				"than standard jitter  = %d\n", g_sensor_info_iio_ext[sensor_index].tag,
				standard_dev, MAX_JITTER);
			set_test_state(FAILED);
			return -1;
		}
		log_msg_and_exit_on_error(DEBUG, "Jitter measured for device %s is %f\n",
			g_sensor_info_iio_ext[sensor_index].tag, standard_dev);
		return 0;
	}   
	return 0; 
}
/* signal data ready for polling mode sensors 
** in order to simulate a frequency for reading 
** samples
*/
void* thread_routine(void* params) {
	int sensor_index;
	int fd;
	float freq;
	int64_t duration;
	struct timespec delay;
	time_t start_time;
	time_t final_time;

	sensor_index = (int)params;
	freq = g_sensor_info_iio_ext[sensor_index].data_rate;
	duration = CONVERT_SEC_TO_NANO(1/freq);
	
	set_timestamp(&delay, duration);
	
	time(&start_time); 
	time(&final_time); 
	fd = g_sensor_info_iio_ext[sensor_index].write_fd;
	
	/* +2 seconds for sync */ 
	while(difftime(final_time, start_time) <= (TIME_TO_MEASURE_SECS + 2)) { 
		if (write(fd, DATA_READY_SIGNAL, 1) == -1) {
			log_msg_and_exit_on_error(ERROR, "Can't write data in fifo for %s\n",
				g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
		}
		nanosleep(&delay, &delay);
		time(&final_time); 
	}
	if (close(fd) == -1) {
		log_msg_and_exit_on_error(ERROR, "Error closing fd %d for writing for device %s: %s\n", 
			fd, g_sensor_info_iio_ext[sensor_index].tag, strerror(errno));
		set_test_state(FAILED);
	}            
	else{
		log_msg_and_exit_on_error(VERBOSE, "Closed fd for device %s\n",
			g_sensor_info_iio_ext[sensor_index].tag);
	}
	
	return NULL;   

}
/* initialize structures, frequency
** and reading fds used in tests which measure timestamp
*/
bool generic_initialize(void* key, void* value, void* context) {
	char sysfs_path[PATH_MAX];
	int sensor_index;
	time_attributes_struct* time_attributes;
	int enabled;
	timestamp_info_struct* timestamp_info;
	int fd;
	int dev_num;
	Hashmap* map_sensor_index_values;
	struct epoll_event ev;

	map_sensor_index_values = (Hashmap*)context;
	time_attributes = (time_attributes_struct*)value;
	sensor_index = (int)key;
	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	memset(sysfs_path, '\0', PATH_MAX);

	if (g_sensor_info_iio_ext[sensor_index].mode == MODE_POLL) {
		log_msg_and_exit_on_error(ERROR, "This test is not available for %s!\n",
			g_sensor_info_iio_ext[sensor_index].tag);
		set_test_state(SKIPPED);
		return true;
	}
	
	if (set_freq(sensor_index, time_attributes->freq) == -1) {
		return true;
	}
				
	time_attributes->freq = g_sensor_info_iio_ext[sensor_index].data_rate;
	hashmapPut(map_sensor_index_to_time_attributes, (void*)sensor_index,
		(void*)time_attributes);


	snprintf(sysfs_path, PATH_MAX, ENABLE_PATH, dev_num);
	if (sysfs_read_int(sysfs_path, &enabled) == -1) {
		log_msg_and_exit_on_error(ERROR, "Can't read value from %s\n", sysfs_path); 
		set_test_state(FAILED);
		return true;
	}
	if (!enabled) {
		if (activate_sensor(sensor_index, 1) == -1) {
			return true;
		}    
	}

	timestamp_info = (timestamp_info_struct*)malloc(sizeof(timestamp_info_struct));
	if (timestamp_info == NULL) {
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		set_test_state(FAILED);
		exit(-1);
	}
	timestamp_info->all_consec_timestamps_diff = 0;
	timestamp_info->counter = 0;
	
	hashmapPut(map_sensor_index_values, (void*)sensor_index, (void*)timestamp_info);
	g_sensor_info_iio_ext[sensor_index].last_timestamp = -1;

	snprintf(sysfs_path, PATH_MAX, DEV_FILE_PATH, dev_num);
	fd = open(sysfs_path, O_RDONLY);
	
	if (fd == -1) {
		log_msg_and_exit_on_error(ERROR, "Error opening file iio:device%d: %s\n",
			dev_num, strerror(errno));
		set_test_state(FAILED);
		return true;
	}
	ev.data.fd = fd;
	ev.events = EPOLLIN;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		log_msg_and_exit_on_error(ERROR, "Error epoll_ctl ADD for iio:device%d: %s\n",
			g_sensor_info_iio_ext[sensor_index].dev_num, strerror(errno));
		set_test_state(FAILED);        
		return true;
	}
	
	g_sensor_info_iio_ext[sensor_index].read_fd = fd;    
	hashmapPut(map_fd_to_sensor_index, (void*)fd, (void*)sensor_index);
	
	return true;
}
/* initialize structures, frequency, reading fds
** used in standard deviation tests  
** and start threads for polling mode sensors
*/
bool standard_deviation_initialize(void* key, void* value, void* context) {
	
	int fd;
	int dev_num;
	int return_value;
	int counter;  
	int i;
	int num_channels;
	int mem_counter;
	int sensor_index;
	float standard_dev;
	float max_dev;
	char sysfs_path[PATH_MAX];
	struct epoll_event ev;
	standard_deviation_struct* st_dev_info;
	int enabled;
	int pfd[2];
	Hashmap *sensor_info;
	pthread_t thread;
		
	/*if returns false the other sensors won't be analysed */
	sensor_index = (int)key;
	max_dev = get_standard_deviation_value(sensor_index);
	if (max_dev == -1) {
		log_msg_and_exit_on_error(ERROR, "This test is not available for %s!\n",
			g_sensor_info_iio_ext[sensor_index].tag);
		set_test_state(SKIPPED);
		return true;
	}

	num_channels = g_sensor_info_iio_ext[sensor_index].num_channels;
	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	sensor_info = (Hashmap*) context;
	memset(sysfs_path, '\0', PATH_MAX);

	st_dev_info = (standard_deviation_struct*)malloc(sizeof(standard_deviation_struct));    
	if (st_dev_info == NULL) {
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		set_test_state(FAILED);
		exit(-1);
	}
	st_dev_info->channels_values = (float**)malloc(sizeof(float*) * num_channels);
	if (st_dev_info->channels_values == NULL) {
			log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
			set_test_state(FAILED);
			exit(-1);
	}
	for (i = 0; i < num_channels; i++) {
		st_dev_info->channels_values[i] = (float *)malloc(COUNTER * sizeof(float));
		if (st_dev_info->channels_values[i] == NULL) {
			log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
			set_test_state(FAILED);
			exit(-1);
		}
	}

	st_dev_info->channels_values_size = COUNTER;
	st_dev_info->counter = 0;

	hashmapPut(sensor_info, (void*)sensor_index, (void*)st_dev_info);

	if (set_cdd_freq(sensor_index) == -1)
		return true;
	
	/* sensors in trigger mode */
	if (g_sensor_info_iio_ext[sensor_index].mode == MODE_TRIGGER) {
		snprintf(sysfs_path, PATH_MAX, ENABLE_PATH, dev_num);
		if (sysfs_read_int(sysfs_path, &enabled) == -1) {
			log_msg_and_exit_on_error(ERROR, "Can't read value from %s\n", sysfs_path); 
			set_test_state(FAILED);
			return true;
		}
		if (!enabled) {
			if (activate_sensor(sensor_index, 1) == -1) {
				return true;
			}  
		}

		snprintf(sysfs_path, PATH_MAX, DEV_FILE_PATH, dev_num);
		fd = open(sysfs_path, O_RDONLY);
		if (fd == -1) {
			log_msg_and_exit_on_error(ERROR, "Error opening file iio:device%d: %s\n",
				dev_num, strerror(errno));  
			set_test_state(FAILED);
			return true;
		}
	}
	/* sensors in polling mode => use threads and pipes to simulate 
	** a frequency for reading samples
	*/
	else{
		if (pipe(pfd) == -1) {
			log_msg_and_exit_on_error(ERROR, "Can't create pipe for polling sensor %s\n",
				g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
			return true;
		}
		g_sensor_info_iio_ext[sensor_index].write_fd = pfd[WRITE];
		if (pthread_create(&thread, NULL, &thread_routine, (void*)sensor_index)) {
			log_msg_and_exit_on_error(ERROR, "Can't create thread for sensor %s\n",
				g_sensor_info_iio_ext[sensor_index].tag);
			set_test_state(FAILED);
			return true;
		}
		threads[sensor_index] = thread;
		fd = pfd[READ];
	}
	
	g_sensor_info_iio_ext[sensor_index].read_fd = fd;
	g_sensor_info_iio_ext[sensor_index].last_timestamp = -1;    

	ev.data.fd = fd;
	ev.events = EPOLLIN;
	if (epoll_ctl (epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		log_msg_and_exit_on_error(ERROR, "Error epoll_ctl ADD for iio:device%d: %s\n",
			dev_num, strerror(errno));
		set_test_state(FAILED);
		return true;
	}
	hashmapPut(map_fd_to_sensor_index, (void*)fd, (void*)sensor_index);    

	return true; 
}

/* initialize structures, frequency
** and reading fds used in jitter tests
*/
bool jitter_initialize(void* key, void* value, void* context) {
	int sensor_index;
	char sysfs_path[PATH_MAX];
	jitter_struct* jitter_info;
	Hashmap *sensor_info;
	int fd;
	int dev_num; 
	struct epoll_event ev;
	int enabled;

	sensor_index = (int)key;
	dev_num = g_sensor_info_iio_ext[sensor_index].dev_num;
	sensor_info = (Hashmap*) context;
	memset(sysfs_path, '\0', PATH_MAX);

	if (g_sensor_info_iio_ext[sensor_index].mode == MODE_POLL) {
		log_msg_and_exit_on_error(ERROR, "This test is not available for %s!\n",
			g_sensor_info_iio_ext[sensor_index].tag);
		set_test_state(SKIPPED);
		return true;
	}   

	jitter_info = (jitter_struct*)malloc(sizeof(jitter_struct));
	 if (jitter_info == NULL) {
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		set_test_state(FAILED);
		exit(-1);
	}

	jitter_info->timestamp_values = (int64_t *)malloc(COUNTER * sizeof(int64_t));
	if (jitter_info->timestamp_values == NULL) {
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		set_test_state(FAILED);
		exit(-1);
	}
	jitter_info->timestamp_values_size = COUNTER;
	jitter_info->counter = 0;

	hashmapPut(sensor_info, (void*)sensor_index, (void*)jitter_info);

	if (set_cdd_freq(sensor_index) == -1) {
		return true;
	}

	snprintf(sysfs_path, PATH_MAX, ENABLE_PATH, dev_num);
	if (sysfs_read_int(sysfs_path, &enabled) == -1) {
		log_msg_and_exit_on_error(ERROR, "Can't read value from %s\n", sysfs_path); 
		set_test_state(FAILED);
		return true;
	}
	if (!enabled) {
		if (activate_sensor(sensor_index, 1) == -1) {
			return true;
		}  
	}
	snprintf(sysfs_path, PATH_MAX, DEV_FILE_PATH, dev_num);
	fd = open(sysfs_path, O_RDONLY);
	if (fd == -1) {
		log_msg_and_exit_on_error(ERROR, "Error opening file iio:device%d: %s\n",
			dev_num, strerror(errno));   
		set_test_state(FAILED);
		return true;
	}

	g_sensor_info_iio_ext[sensor_index].read_fd = fd;
	g_sensor_info_iio_ext[sensor_index].last_timestamp = -1;
	ev.data.fd = fd;
	ev.events = EPOLLIN;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		log_msg_and_exit_on_error(ERROR, "Error epoll_ctl ADD for iio:device%d: %s\n",
			dev_num, strerror(errno));
		set_test_state(FAILED);
		return true;
	}
	hashmapPut(map_fd_to_sensor_index, (void*)fd, (void*)sensor_index);
	return true;    
}

/* call compute phase and close fds */
bool generic_finalize(void* key, void* value, void* context) {
	int sensor_index;
	int fd;

	sensor_index = (int)key;
	fd = g_sensor_info_iio_ext[sensor_index].read_fd;
	
	int (*wrapper) (int, void*, int) = (int (*) (int, void*, int))context;
	
	wrapper(sensor_index, value, FINALIZE);
	if (fd != -1) {
		if (close(fd) == -1) {
			log_msg_and_exit_on_error(ERROR, "Error closing fd %d for device %s: %s\n", 
				fd, g_sensor_info_iio_ext[sensor_index].tag, strerror(errno));
			set_test_state(FAILED);
		}    
		else{
			log_msg_and_exit_on_error(VERBOSE, "Closed fd %d for device %s\n", fd,
				g_sensor_info_iio_ext[sensor_index].tag);
		}
		g_sensor_info_iio_ext[sensor_index].read_fd= -1;
	}
	return true;
}

/* initialize, poll and call specific wrapper functions when data ready */
int poll_sensors(bool (*initialize) (void*, void*, void*),
	int (*wrapper) (int, void*, int), int duration) {
	
	int duration_to_millisecs;
	struct epoll_event ret_ev;
	time_t start_time, final_time;
	int sensor_index;
	Hashmap *map_sensor_index_values;
	
	map_fd_to_sensor_index = hashmapCreate(HASHMAP_SIZE, hash, intEquals);
	if (map_fd_to_sensor_index == NULL) {
		log_msg_and_exit_on_error(ERROR, "Error creating Hashmap!\n");
		set_test_state(FAILED);
		return -1;
	}

	map_sensor_index_values = hashmapCreate(HASHMAP_SIZE, hash, intEquals);
	if (map_sensor_index_values == NULL) {
		log_msg_and_exit_on_error(ERROR, "Error creating Hashmap!\n");
		set_test_state(FAILED);
		return -1;
	}

   
	duration_to_millisecs = CONVERT_SEC_TO_MILLI(duration);
	epfd = epoll_create(hashmapSize(map_sensor_index_to_time_attributes));
	if (epfd == -1) {
		log_msg_and_exit_on_error(ERROR, "Error epoll_create: %s\n", strerror(errno));
		set_test_state(FAILED);
		return -1;
	}

	hashmapForEach(map_sensor_index_to_time_attributes, initialize,
		(void*)map_sensor_index_values);
	
	/* no device can be tested */
	if ((hashmapSize(map_fd_to_sensor_index) == 0) || (hashmapSize(map_sensor_index_values) == 0))
		return -1;
	time(&start_time);
	time(&final_time); 
	while((difftime(final_time, start_time) <= duration)) {
		if (epoll_wait(epfd, &ret_ev, 1, duration_to_millisecs) == -1) {
			log_msg_and_exit_on_error(ERROR, "Error epoll_wait: %s\n", strerror(errno));
			set_test_state(FAILED); 
			return -1;
		}
		if ((ret_ev.events & EPOLLIN) != 0) {
			sensor_index = (int)hashmapGet(map_fd_to_sensor_index, (void*)ret_ev.data.fd);
			void* timestamp_info = (void*)hashmapGet(map_sensor_index_values, (void*)sensor_index);
			wrapper(sensor_index, timestamp_info, PROCESS);
		}
		time(&final_time);
	}
	
	hashmapForEach(map_sensor_index_values, generic_finalize, (void*)wrapper);
	
	if (close(epfd) == -1) {
		log_msg_and_exit_on_error(ERROR, "Error closing fd for epoll: %s\n", strerror(errno));
		set_test_state(FAILED);
		return -1;
	}
	log_msg_and_exit_on_error(VERBOSE, "Closed fd for epoll\n");
	free(map_sensor_index_values);
	free(map_fd_to_sensor_index);
	return 0;         
	
}

