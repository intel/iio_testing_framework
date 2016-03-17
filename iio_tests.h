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
#ifndef __IIO_TESTS_H__
#define __IIO_TESTS_H__

int poll_sensors(bool (*initialize) (void*, void*, void*), int (*wrapper) (int, void*, int), int duration);
bool generic_initialize(void* key, void* value, void* context);
bool jitter_initialize(void* key, void* value, void* context);
bool standard_deviation_initialize(void* key, void* value, void* context);
bool generic_finalize(void* key, void* value, void* context);
int standard_deviation_wrapper(int sensor_index, void* counter_timestamp, int stage);
int check_client_average_delay_wrapper(int sensor_index, void* counter_timestamp, int stage);
int check_client_delay_wrapper(int sensor_index, void* counter_timestamp, int stage);
int measure_freq_wrapper(int sensor_index, void* counter_timestamp, int stage);
int check_sample_timestamp_average_difference_wrapper(int sensor_index, void* counter_timestamp, int stage);
int check_sample_timestamp_difference_wrapper(int sensor_index, void* counter_timestamp, int stage);
int test_jitter_wrapper(int sensor_index, void* counter_timestamp, int stage);

#endif