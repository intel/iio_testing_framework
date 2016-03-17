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
#include "iio_parser.h"
#include "iio_utils.h"
#include "iio_sample_format.h"
#include "iio_enumeration.h"

int current_fd;
int nr_test;
level log_level;

int main(int argc, char *argv[]) {
	int sensor_index, dev_num, counter, max_delay, duration, msg_fd;
	float freq;
	char sysfs_path[PATH_MAX];
	char buffer[BUFFER_SIZE];

	nr_test = 0;
	log_level = atoi(argv[2]);
	sprintf(sysfs_path, "%s%s", argv[6], TESTS_MSG);

	msg_fd = open(sysfs_path, O_CREAT|O_WRONLY|O_TRUNC, S_IWOTH);
	if (msg_fd == -1) {
		printf("Cannot open %s (%s)\n", sysfs_path, strerror(errno));
		exit(-1);
	}

	current_fd = msg_fd;

	enumerate_sensors();
	set_sample_format();
	if (argc == 7) 
		return read_tests(argv[4], argv[6]);

	/* single test from cmd line */
	else {
		tests = (test_info_t*)malloc(1 * sizeof(test_info_t));
		if (tests == NULL) {   
			log_msg_and_exit_on_error(FATAL, "Out of memory!\n");
			exit(-1);
		}
		tests[nr_test].state = PASSED;
		parse_cmd(argv[8]);
		if (tests[nr_test].state == PASSED) {
			log_msg_and_exit_on_error(NOTHING, "Test has passed!\n");
		}
		else if (tests[nr_test].state == FAILED) {
			log_msg_and_exit_on_error(NOTHING, "Test has failed!\n");
		}
		else if (tests[nr_test].state == SKIPPED)
			log_msg_and_exit_on_error(NOTHING, "Test was skipped!\n");
  
	}
	free(tests);
	return 0;
}
