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
#include <sys/stat.h> 
#include <fcntl.h> 
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include "cutils/hashmap.h"
#include "iio_utils.h"
#include "iio_control.h"
#include "iio_parser.h"
#include "iio_tests.h"
#include "iio_control_frequency.h"
#include "iio_set_trigger.h"

test_info_t *tests;
Hashmap *map_sensor_index_to_time_attributes;
int current_fd;
static int duration;
static int counter;
/* get frequency or delay for each sensor and duration or counter
** for each command
*/
int get_sensors_time_attributes(char* cmd) {
	char *field;
	int nr_bytes;
	int sensor_index;
	time_attributes_struct* time_attributes;
	parsing_state state;

	field = (char*)malloc(BUFFER_SIZE);
	if (field == NULL) {
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		set_test_state(FAILED);
		exit(-1); 
	}
	map_sensor_index_to_time_attributes = hashmapCreate(HASHMAP_SIZE, hash, intEquals);
	if (map_sensor_index_to_time_attributes == NULL) {
		log_msg_and_exit_on_error(ERROR, "Error creating Hashmap!\n");
		set_test_state(FAILED);
		return -1;
	}
	
	time_attributes = (time_attributes_struct*)malloc(sizeof(time_attributes_struct));
	if (time_attributes == NULL) {
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		set_test_state(FAILED);
		exit(-1);
	}
	time_attributes->freq = 0;
	time_attributes->max_delay = 0; 
	sensor_index = -1;
	duration = 0;
	counter = 0;
	state = INIT_STATE;

	while ( sscanf(cmd, "%s%n", field, &nr_bytes) == 1 ) {
		if (isdigit(*field)) {
			if (state == INIT_STATE) {
				log_msg_and_exit_on_error(ERROR, "Wrong format for test!\n");
				set_test_state(FAILED);
				return -1;
			}
			else if (state == FREQ_STATE) {
				state = DELAY_STATE;
				time_attributes = (time_attributes_struct*)malloc(sizeof(time_attributes_struct));
				if (time_attributes == NULL) {
					log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
					set_test_state(FAILED);
					exit(-1);
				}
				time_attributes->freq = atof(field);
				time_attributes->max_delay = 0;
			}
			else if (state == DELAY_STATE) {
				state = SENSOR_TAG_STATE;
				time_attributes->max_delay = atoi(field);
			
			}
			else if (state == DURATION_STATE) {
					duration = atoi(field);
					counter = 0;
					state = FINISH_STATE;
			}
			else if (state == COUNTER_STATE) {
				counter = atoi(field);
				state = FINISH_STATE;
			} 
			
		}
		/* each tag(duration, counter, freq, delay) set a parsing state */
		else{
			if (sensor_index != -1) {
				hashmapPut(map_sensor_index_to_time_attributes, (void*)sensor_index,
					(void*)time_attributes);
			}
			if (strncmp(field, "duration", nr_bytes) == 0) {
				cmd += nr_bytes; 
				if ( *cmd != ' ' ) {
					log_msg_and_exit_on_error(ERROR, "Wrong format for test!\n");
					set_test_state(FAILED);
					return -1;
				}
				++cmd;
				state = DURATION_STATE;
				continue;
			}
			if (strncmp(field, "counter", nr_bytes) == 0) {
				cmd += nr_bytes; 
				if ( *cmd != ' ' ) {
					log_msg_and_exit_on_error(ERROR, "Wrong format for test!\n");
					set_test_state(FAILED);
					return -1;
				}
				++cmd;
				state = COUNTER_STATE;
				continue;
			}
			if (strncmp(field, "freq", nr_bytes) == 0) {
				cmd += nr_bytes; 
				if ( *cmd != ' ' ) {
					log_msg_and_exit_on_error(ERROR, "Wrong format for test!\n");
					set_test_state(FAILED);
					return -1;
				}
				++cmd;
				state = FREQ_STATE;
				continue;
			}

			if (strncmp(field, "delay", nr_bytes) == 0) {
				cmd += nr_bytes; 
				if ( *cmd != ' ' ) {
					log_msg_and_exit_on_error(ERROR, "Wrong format for test!\n");
					set_test_state(FAILED);
				}
				++cmd;
				state = DELAY_STATE;
				continue;
			}            
			sensor_index = get_index_from_tag(field);
			if (sensor_index == -1) {
				log_msg_and_exit_on_error(ERROR, "Device %s doesn't exist!\n", field);
				set_test_state(SKIPPED); 
			}
		}
		cmd += nr_bytes;  
		if ( *cmd != ' ' ) {
			break;
		}
		++cmd; 
		
	}
	free(field);
	if (sensor_index != -1) {
		hashmapPut(map_sensor_index_to_time_attributes, (void*)sensor_index, (void*)time_attributes);
	}
	return 0;
}
/* parse each command and call proper function */
int parse_cmd(char* cmd) {
	char *action;
	char *field;
	int sensor_index;
	int nr_bytes;
	int value;
	time_attributes_struct* time_attributes;
	parsing_state state;
	
	action = (char*)malloc(BUFFER_SIZE);
	if (action == NULL) {
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		set_test_state(FAILED);
		exit(-1);
	}
	
	
	sscanf(cmd, "%s%n", action, &nr_bytes);
	/* for action without parameters */
	if (strncmp(action, "list", 4) == 0) {
		if (strncmp(action + 5, "trig", 4) == 0) {
			list_triggers();
		}
		else{
			list_sensors();
		}
		free(action);
		return 0;
	}
	else if (strncmp(action, "clean", 5) == 0) {
		free(action);
		return clean_up_sensors();
	}
	else if (strncmp(action, "activate_all_sensors", 12) == 0) {
		log_msg_and_exit_on_error(DEBUG, "activate all");
		free(action);
		return activate_all_sensors(1);        
	} 

	else if (strncmp(action, "deactivate_all_sensors", 14) == 0) {
		log_msg_and_exit_on_error(DEBUG, "deactivate all");
		free(action);
		return activate_all_sensors(0);      
	} 

	cmd += (nr_bytes + 1);
	if (get_sensors_time_attributes(cmd) < 0)
		return -1;


	if (strncmp(action, "check", 5) == 0) {
		
		/* check sample tests */
		if (strncmp(action + 6, "sample", 6) == 0) {
			if (strncmp(action + 23, "average", 7) == 0) {
				poll_sensors(generic_initialize, check_sample_timestamp_average_difference_wrapper, duration);
			}
			else{
				poll_sensors(generic_initialize, check_sample_timestamp_difference_wrapper, duration);
			}
		}
		/* check client tests */    
		else if (strncmp(action + 6, "client", 6) == 0) {
			if (strncmp(action + 13, "average", 7) == 0) {
				poll_sensors(generic_initialize, check_client_average_delay_wrapper, duration);
			}
			else{
				poll_sensors(generic_initialize, check_client_delay_wrapper, duration);
			}

		}
		else if (strncmp(action + 6, "freq", 4) == 0) {
			poll_sensors(generic_initialize, measure_freq_wrapper, duration);
		}
		else if (strncmp(action + 6, "channels", 8) == 0) {
			hashmapForEach(map_sensor_index_to_time_attributes, check_channels_wrapper, NULL);
		}    
	}
	else if (strncmp(action, "jitter", 6) == 0) {
		poll_sensors(jitter_initialize, test_jitter_wrapper, TIME_TO_MEASURE_SECS);
	}
	else if (strncmp(action, "standard", 8) == 0) {
		poll_sensors(standard_deviation_initialize, standard_deviation_wrapper, TIME_TO_MEASURE_SECS);
	}

	/* set tests */
	else if (strncmp(action, "set", 3) == 0) {
		hashmapForEach(map_sensor_index_to_time_attributes, set_freq_wrapper, (void*)duration);   
	}

	/* activate tests */
	else if (strncmp(action, "activ", 5) == 0) {
		if (strlen(action) == 8) {
			value = 1;
			hashmapForEach(map_sensor_index_to_time_attributes, activate_sensor_wrapper, (void*)value);    
		}
		else if (strlen(action) == 19) {
			hashmapForEach(map_sensor_index_to_time_attributes, activate_deactivate_sensor_wrapper, (void*)counter);
		} 
		else{
			activate_deactivate_all_sensors(counter);
		}
	}
	else if (strncmp(action, "deactiv", 7) == 0) {
		value = 0;
		hashmapForEach(map_sensor_index_to_time_attributes, activate_sensor_wrapper, (void*)value);
	}
	else{
		/* this action is not defined */ 
		log_msg_and_exit_on_error(ERROR, "Action %s is not defined!\n", action);
		set_test_state(FAILED);
	}

	hashmapFree(map_sensor_index_to_time_attributes);
	free(action);

	return 0; 
}
/* parse each test separately */
int parse_test(char* test, char path[PATH_MAX], int type) {
	char *line;
	char buffer[BUFFER_SIZE];
	char sysfs_path[PATH_MAX];
	char start_time[TIME_SIZE];
	char cmd[BUFFER_SIZE];
	time_t rawtime;
	struct tm *timeinfo;

	/* take time data and create file for logs info */
	if (type == DESCRIPTION) {
		time (&rawtime);
		timeinfo = localtime (&rawtime);
		sprintf(start_time, "%d-%d %d:%d:%d.0", timeinfo->tm_mon + 1, 
			timeinfo->tm_mday, timeinfo->tm_hour, 
			timeinfo->tm_min, timeinfo->tm_sec);

		sprintf(sysfs_path, "%s%s%d", path, TESTS_LOGS, nr_test);
		tests[nr_test].log_fd = open(sysfs_path, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXO);
		if (tests[nr_test].log_fd == -1) {
			log_msg_and_exit_on_error(ERROR, "Cannot open %s (%s)\n", sysfs_path, strerror(errno));
			set_test_state(FAILED);
			return -1;
		}
		sprintf(cmd, "logcat -t \"%s\" -f \"%s\"", start_time, sysfs_path);
		memcpy(test + strlen(test), "\n", 1);
		printf("%s\n", test);
		log_msg_and_exit_on_error(NOTHING, test);
		line = strtok(test, "\n");
		tests[nr_test].description = strdup(line);
		if (tests[nr_test].description == NULL) {
			log_msg_and_exit_on_error(ERROR, "Can't copy message for this test %d: %s\n", nr_test, strerror(errno));
			set_test_state(FAILED);
			
		}
		else{
			tests[nr_test].state = PASSED;
		}
		return 0;
	} 
	line = strtok(test, "\n");
	while(line != NULL) {
		parse_cmd(line);
		line = strtok(NULL, "\n");
	}
	system(cmd);
	if (close(tests[nr_test].log_fd) == -1) {
		log_msg_and_exit_on_error(ERROR, "Cannot close %s (%s)\n", sysfs_path, strerror(errno));
		set_test_state(FAILED);
		return -1;
	}
	if (tests[nr_test].state == PASSED) {
		log_msg_and_exit_on_error(NOTHING, "Test has passed!\n\n\n");
	}
	else if (tests[nr_test].state == FAILED) {
		log_msg_and_exit_on_error(NOTHING, "Test has failed!\n\n\n");
	}
	else{
		log_msg_and_exit_on_error(NOTHING, "Test was skipped!\n\n\n");
	}
	
	return 0;
}
/* print each test result in results file */
int print_tests_results(char path[PATH_MAX]) {
	int results_fd;
	char buffer[BUFFER_SIZE];
	char sysfs_path[PATH_MAX];
	int msg_fd;
	int i;

	sprintf(sysfs_path, "%s%s", path, TESTS_RESULTS);
	results_fd = open(sysfs_path, O_CREAT|O_WRONLY|O_TRUNC, S_IWOTH);
	if (results_fd == -1) {
		log_msg_and_exit_on_error(FATAL, "Cannot open %s (%s)\n", sysfs_path, strerror(errno));
		exit(-1);
	}
	snprintf(buffer, sizeof(buffer), "Tests results\n");
	msg_fd = current_fd;
	current_fd = results_fd;
	log_msg_and_exit_on_error(NOTHING, buffer);
	
	for (i = 0; i < nr_test; ++i) {
		if (tests[i].state == PASSED) {
			log_msg_and_exit_on_error(NOTHING, "%s\n\t\t\t passed\n",tests[i].description);
		}
		else if (tests[i].state == FAILED) {
			log_msg_and_exit_on_error(NOTHING, "%s\n\t\t\t failed\n",tests[i].description);
		}
		else{
			log_msg_and_exit_on_error(NOTHING, "%s\n\t\t\t skipped\n",tests[i].description);
		}
	}
	
	for (i = 0; i < nr_test; ++i) {
		free(tests[i].description);
	}
	free(tests);
	current_fd = msg_fd;
	
	if (close(results_fd) == -1) {
		log_msg_and_exit_on_error(ERROR, "Cannot close %s (%s)\n", sysfs_path, strerror(errno));
		return -1;
	} 
	return 0;  

}
/* read all tests and parse them separately */
int read_tests(char suit_path[PATH_MAX], char path[PATH_MAX])
{
	ssize_t byteRead;    
	ssize_t bytesRead;                         
	char* buf;
	char* new_buf;
	char sysfs_path[PATH_MAX];
	char buffer[BUFFER_SIZE];
	char ch;
	int fd;
	int msg_fd;
	int counter;
	int test_counter;
	int i;

	nr_test = 0;
	bytesRead = 0;
	counter = BUFFER_SIZE;
	test_counter = NUMTESTS;
	
   
	msg_fd = current_fd;

	sprintf(sysfs_path, "%s%s", path, TESTS_RESULTS);
	fd = open(suit_path, O_RDONLY);
	if (fd == -1) {
		log_msg_and_exit_on_error(FATAL, "Cannot open %s (%s)\n", path, strerror(errno));
		exit(-1);
	}  
	tests = (test_info_t*)malloc(NUMTESTS * sizeof(test_info_t));
	if (tests == NULL) {   
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		exit(-1);
	}
	new_buf = (char*)malloc(BUFFER_SIZE);
	if (new_buf == NULL) {   
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		exit(-1);
	}
	buf = (char*)malloc(BUFFER_SIZE);
	if (buf == NULL) {   
		log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
		exit(-1);
	}
	for (i = 0; i < nr_test; ++i) {
		tests[i].log_fd = -1;
	}
	if (sysfs_write_str_fd(current_fd, "\n\n\n") == -1)
		exit(-1);
	for (;;) {
		byteRead = read(fd, &ch, 1);

		if (byteRead == -1) {
			if (errno == EINTR)         
				continue;
			else{
				log_msg_and_exit_on_error(FATAL, "Cannot read: (%s)\n", strerror(errno)); 
				exit(-1);
			}            

		} else if (byteRead == 0) {     
			break;
		}
		else {                       
			
			if (ch == '{') {
				parse_test(buf, path, DESCRIPTION);
				memset(buf, '\0', BUFFER_SIZE);
				bytesRead = 0;
				continue;
			}            

			if (ch == '}') {
				parse_test(buf, path, CONTENT);
				memset(buf, '\0', BUFFER_SIZE);
				bytesRead = 0;
				nr_test ++;
				if (nr_test >= test_counter) {
					test_counter *= 2;
					tests = (test_info_t*)realloc(tests, test_counter * sizeof(test_info_t));
					if (tests == NULL) {
						log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
						exit(-1);
					}
				}
				continue;
			}

			buf[bytesRead] = ch;
			bytesRead ++;
			if (bytesRead >= counter) {
				counter *= 2;
				new_buf = (char*)realloc(buf, counter);
				if (new_buf == NULL) {
					log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
					exit(-1);
				}
				buf = new_buf;
			}
		   
		}
	}
 
	free(buf);
	free(new_buf);
 
	if (print_tests_results(path) < 0) {
		log_msg_and_exit_on_error(ERROR, "Can't write in  results in tests results file!\n");
	}

	if (close(msg_fd) == -1) {
		printf("Cannot close %s (%s)\n", sysfs_path, strerror(errno));
		return -1;
	}
 
	return 0;

}
