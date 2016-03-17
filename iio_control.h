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

#ifndef __IIO_CONTROL_H__
#define __IIO_CONTROL_H__

int enable_buffer(int sensor_index, int enabled);
int enable_all_buffers(int value);
int clean_up_sensors(void);
int activate_sensor(int sensor_index, int value);
bool activate_sensor_wrapper(void* key, void* value, void* context);
int activate_deactivate_sensor(int sensor_index, int counter);
bool activate_deactivate_sensor_wrapper(void* key, void* value, void* context); 
int activate_all_sensors(int value);
int activate_deactivate_all_sensors(int counter);
int get_index_from_dev_num(int dev_num);
int get_index_from_tag(char * tag);
int get_data_polling_mode(int sensor_index);
int get_data_triggered_mode(int sensor_index);
void list_sensors(void);
int check_channels(int sensor_index);
bool check_channels_wrapper(void* key, void* value, void* context);

#endif
