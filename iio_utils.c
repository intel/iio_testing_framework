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
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include "cutils/hashmap.h"
#include "iio_utils.h"

/* write content in a file given by it's fd */
int sysfs_write_fd(int fd, const void *buf, const int buf_len)
{
	int len;
	char buffer[BUFFER_SIZE];
	if (!buf || buf_len < 1) {
		sprintf(buffer, "%s", "[DEBUG] Wrong buffer!\n");
		len = write(fd, buffer, strlen(buffer));		
		return -1;
	}

	len = write(fd, buf, buf_len);

	if (len == -1) {
		printf("[FATAL] Cannot write: (%s)\n", strerror(errno));
	}

	return len;
}

int sysfs_write_str_fd(int fd, const char *str)
{
	if (!str || !str[0])
		return -1;

	return sysfs_write_fd(fd, str, strlen(str));
}

int log_msg_and_exit_on_error(level msg_level, const char *format, ...) {
	va_list arg;
	int len;
	/* Write the error message */
	char msg[BUFFER_SIZE];
	if(msg_level <= log_level) {
		va_start(arg, format);	
		memset(msg, '\0', BUFFER_SIZE);
		if(msg_level == NOTHING) {
		}
		if(msg_level == VERBOSE) {
			snprintf(msg, BUFFER_SIZE, "[VERBOSE] ");
		}
		if(msg_level == DEBUG) {
			snprintf(msg, BUFFER_SIZE, "[DEBUG] ");
		}	
		if(msg_level == ERROR) {
			snprintf(msg, BUFFER_SIZE, "[ERROR] ");
		}
		if(msg_level == FATAL) {
			snprintf(msg, BUFFER_SIZE, "[FATAL] ");
		}
		vsprintf(msg + strlen(msg), format, arg);
		len = write(current_fd, msg, strlen(msg));
		/* exit if data can't be write */
		if (len == -1) {
			printf("[FATAL] Cannot write: (%s)\n", strerror(errno));
			exit(-1);
		}

		va_end(arg);
		return len;
	}
	return 0;
}

void set_test_state(test_state type) {
	tests[nr_test].state = type;
}
/* read content from a file given by it's fd */
int sysfs_read_from_fd(int fd, char *buf, int buf_len)
{
	int len;
	char buffer[BUFFER_SIZE];
	if (!buf || buf_len < 1)
		return -1;

	
	len = read(fd, buf, buf_len);


	if (len == -1) {
		log_msg_and_exit_on_error(DEBUG, "Cannot read: (%s)\n",
			strerror(errno));
	}
	return len;
}

/* read content from a file given by it's path */
int sysfs_read(const char path[PATH_MAX], void *buf, int buf_len)
{
	int fd, len;
	char buffer[BUFFER_SIZE];

	if (!path[0] || !buf || buf_len < 1)
		return -1;

	fd = open(path, O_RDONLY);

	if (fd == -1) {
		log_msg_and_exit_on_error(DEBUG, "Cannot open %s (%s)\n", path,
			strerror(errno));
		return -1;
	}

	len = read(fd, buf, buf_len);

	if(close(fd) == -1) {
		log_msg_and_exit_on_error(DEBUG, "Cannot close %s (%s)\n", path,
			strerror(errno));
		return -1;
	}

	if (len == -1) {
		log_msg_and_exit_on_error(DEBUG, "Cannot read from %s (%s)\n", path,
			strerror(errno));
	}
	return len;
}

/* write content in a file given by it's path */
int sysfs_write(const char path[PATH_MAX], const void *buf,
	const int buf_len) {

	int fd, len;
	char buffer[BUFFER_SIZE];

	if (!path[0] || !buf || buf_len < 1)
		return -1;

	fd = open(path, O_WRONLY);

	if (fd == -1) {
		log_msg_and_exit_on_error(DEBUG, "Cannot open %s (%s)\n", path,
			strerror(errno));
		return -1;
	}

	len = write(fd, buf, buf_len);

	
	if (close(fd) == -1) {
		log_msg_and_exit_on_error(DEBUG, "Cannot close %s (%s)\n", path,
			strerror(errno));
	}
	if (len == -1) {
		log_msg_and_exit_on_error(DEBUG, "Cannot write to %s (%s)\n", path,
			strerror(errno));
	}	
	return len;
}

/* read string content from a file given by it's path */
int sysfs_read_str(const char path[PATH_MAX], char *buf, int buf_len)
{
	int len;
	char buffer[BUFFER_SIZE];

	if (!buf || buf_len < 1)
		return -1;

	len = sysfs_read(path, buf, buf_len);

	buf[len == 0 ? 0 : len - 1] = '\0';
	
	return len;
}

/* read specific number type from a file given by it's path */
int sysfs_read_num(const char path[PATH_MAX], void *v,
		void (*str2num)(const char* buf, void *v))
{
	char buf[20];
	char buffer[BUFFER_SIZE];
	int len;

	len = sysfs_read_str(path, buf, sizeof(buf));
	if (len == -1) {
		log_msg_and_exit_on_error(DEBUG, "Cannot read from %s (%s)\n", path,
			strerror(errno));
		return -1;
	}

	str2num(buf, v);
	return 0;
}
void str2int(const char* buf, void *v)
{
	*(int*)v = atoi(buf);
}

void str2float(const char* buf, void *v)
{
	*(float*)v = strtof(buf, NULL);
}

int sysfs_read_int(const char path[PATH_MAX], int *value)
{
	return sysfs_read_num(path, value, str2int);
}


int sysfs_read_float(const char path[PATH_MAX], float *value)
{
	return sysfs_read_num(path, value, str2float);
}

/* write string content in a file given by it's path */
int sysfs_write_str(const char path[PATH_MAX], const char *str)
{
	if (!str || !str[0])
		return -1;

	return sysfs_write(path, str, strlen(str));
}

/* write specific number type in a file given by it's path */
int sysfs_write_int(const char path[PATH_MAX], int value)
{
	char buf[20];
	char buffer[BUFFER_SIZE];
	int len;

	len = snprintf(buf, sizeof(buf), "%d", value);

	return sysfs_write(path, buf, len);
}

int sysfs_write_float(const char path[PATH_MAX], float value)
{
	char buf[20];
	char buffer[BUFFER_SIZE];
	int len;

	len = snprintf(buf, sizeof(buf), "%g", value);

	return sysfs_write(path, buf, len);
}
int64_t get_timestamp_realtime (void)
{
	struct timespec ts = {0};
	clock_gettime(CLOCK_REALTIME, &ts);
	return 1000000000LL * ts.tv_sec + ts.tv_nsec;
}
void set_timestamp (struct timespec *out, int64_t target_ns)
{
	out->tv_sec  = target_ns / 1000000000LL;
	out->tv_nsec = target_ns % 1000000000LL;
}

/* hash function for hashmap used to test multiple 
** devices in the same time 
*/
int hash(void* x_void) {
	unsigned int x = (unsigned int)x_void;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x);
	return x;
}
bool intEquals(void* keyA, void* keyB) {
	int a = (int)keyA;
	int b = (int)keyB;
	return a == b;
}