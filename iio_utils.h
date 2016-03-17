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

#include "iio_common.h"
#ifndef __IIO_UTILS_H__
#define __IIO_UTILS_H__

int sysfs_write_fd(int fd, const void *buf, const int buf_len);
int sysfs_write_str_fd(int fd, const char *str);
int log_msg_and_exit_on_error(level msg_level, const char *format, ...); 
void set_test_state(test_state type);
int sysfs_read_from_fd(int fd, char *buf, int buf_len);
int sysfs_read(const char path[PATH_MAX], void *buf, int buf_len);
int sysfs_write(const char path[PATH_MAX], const void *buf, const int buf_len);
int sysfs_read_str(const char path[PATH_MAX], char *buf, int buf_len);
int sysfs_read_num(const char path[PATH_MAX], void *v, void (*str2num)(const char* buf, void *v));
int sysfs_read_int(const char path[PATH_MAX], int *value);
int sysfs_read_float(const char path[PATH_MAX], float *value);
int sysfs_write_str(const char path[PATH_MAX], const char *str);
int sysfs_write_int(const char path[PATH_MAX], int value);
int sysfs_write_float(const char path[PATH_MAX], float value);
int64_t get_timestamp_realtime(void);
void set_timestamp (struct timespec *out, int64_t target_ns);
int hash(void* x_void);
bool intEquals(void* keyA, void* keyB);
#endif



