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

#ifndef __IIO_SAMPLE_FORMAT_H__
#define __IIO_SAMPLE_FORMAT_H__

int decode_type_spec (const char type_buf[MAX_TYPE_SPEC_LEN], datum_info_t *type_info);
int get_padding_size(int total, int storagebits);
int set_sample_format(void);
int64_t sample_as_int64 (unsigned char* sample, datum_info_t* type);
float scale_value(int sensor_index, int channel, int64_t value);

#endif
