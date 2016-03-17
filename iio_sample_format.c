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
#include "cutils/hashmap.h"
#include "iio_sample_format.h"
#include "iio_enumeration.h"
#include "iio_utils.h"

int get_padding_size(int total, int storagebits) {
	int alignement; 

	alignement = storagebits/8;
	if (((storagebits % 8) == 0) && (total % alignement) != 0) {
		return (alignement - total % alignement);
	}
	else
		return 0;
}

int decode_type_spec (const char type_buf[MAX_TYPE_SPEC_LEN], datum_info_t *type_info) {
	/* Return size in bytes for this type specification, or -1 in error */
	
	unsigned int realbits, storagebits, shift;
	int tokens;
	char sign;
	char endianness;

	/* Valid specs: "le:u10/16>>0", "le:s16/32>>0" or "le:s32/32>>0" */

	tokens = sscanf(type_buf, "%ce:%c%u/%u>>%u", &endianness, &sign, &realbits, &storagebits, &shift);

	if (tokens != 5 || (endianness != 'b' && endianness != 'l') || (sign != 'u' && sign != 's') ||
		realbits > storagebits || (storagebits != 16 && storagebits != 32 && storagebits != 64)) {
			log_msg_and_exit_on_error(ERROR, "Invalid iio channel type spec: %s\n", type_buf);
			set_test_state(FAILED);
			return -1;
	}

	type_info->endianness   =       endianness;
	type_info->sign     =       sign;
	type_info->realbits =   (short) realbits;
	type_info->storagebits  =   (short) storagebits;
	type_info->shift    =   (short) shift;

	return storagebits / 8;
}

int set_sample_format(void) {
	int num_channels;
	int c;
	int i;
	int size;
	int padding;
	char sysfs_path[PATH_MAX];
	channel_info_t channel, timestamp;

	size = 0;
	memset(sysfs_path, '\0', PATH_MAX);

	for (i=0; i< g_sensor_info_size; i++) {
		if (g_sensor_info_iio_ext[i].discovered) {
			if (g_sensor_info_iio_ext[i].mode == MODE_POLL)
				continue;
			/* set sample format for channels */
			num_channels = g_sensor_info_iio_ext[i].num_channels;
			for (c = 0; c < num_channels; c++) {
				channel = g_sensor_info_iio_ext[i].channel_info[c];
				snprintf(sysfs_path, PATH_MAX, CHANNEL_PATH "%s", 
					g_sensor_info_iio_ext[i].dev_num,
					g_sensor_info_iio_ext[i].channel_descriptor[c].type_path); 
				
				if (sysfs_read_str(sysfs_path, channel.type_spec, MAX_TYPE_SPEC_LEN) == -1) {
					log_msg_and_exit_on_error(ERROR, "[%s] : %s", sysfs_path, strerror(errno));
					continue;
				}    
				channel.size = decode_type_spec(channel.type_spec, &channel.type_info);
				if (channel.size == -1) {
					log_msg_and_exit_on_error(ERROR, "Device %s has invalid spec for channel %s!\n",
						g_sensor_info_iio_ext[i].tag,
						g_sensor_info_iio_ext[i].channel_descriptor[c].name);
					continue;
				}
				padding = get_padding_size(size, channel.type_info.storagebits);
				size = size + channel.size + padding;
				g_sensor_info_iio_ext[i].channel_info[c] = channel;
			}

			/* set sample format for timestamp */
			timestamp = g_sensor_info_iio_ext[i].timestamp;
			snprintf(sysfs_path, PATH_MAX, TIMESTAMP_TYPE_PATH, g_sensor_info_iio_ext[i].dev_num); 
			if (sysfs_read_str(sysfs_path, timestamp.type_spec, MAX_TYPE_SPEC_LEN) == -1) {
				log_msg_and_exit_on_error(ERROR, "[%s] : %s", sysfs_path, strerror(errno));
				continue;
			}
			timestamp.size = decode_type_spec(timestamp.type_spec, &timestamp.type_info);
			if (timestamp.size == -1) {
				log_msg_and_exit_on_error(ERROR, "Device %s has invalid spec for timestamp!\n",
					g_sensor_info_iio_ext[i].tag);
				continue;
			}      
			padding = get_padding_size(size, timestamp.type_info.storagebits);
			size = size + timestamp.size + padding;
			g_sensor_info_iio_ext[i].timestamp = timestamp;
			g_sensor_info_iio_ext[i].sample_size = size;
		}
	}    
	return 0;
}
int64_t sample_as_int64 (unsigned char* sample, datum_info_t* type) {
	uint64_t u64;
	int i;
	int zeroed_bits = type->storagebits - type->realbits;
	uint64_t sign_mask;
	uint64_t value_mask;

	u64 = 0;

	if (type->endianness == 'b')
		for (i=0; i<type->storagebits/8; i++)
			u64 = (u64 << 8) | sample[i];
	else
		for (i=type->storagebits/8 - 1; i>=0; i--)
			u64 = (u64 << 8) | sample[i];

	u64 = (u64 >> type->shift) & (~0ULL >> zeroed_bits);

	if (type->sign == 'u')
		return (int64_t) u64; /* We don't handle unsigned 64 bits int */

	/* Signed integer */

	switch (type->realbits) {
		case 0 ... 1:
			return 0;

		case 8:
			return (int64_t) (int8_t) u64;

		case 16:
			return (int64_t) (int16_t) u64;

		case 32:
			return (int64_t) (int32_t) u64;

		case 64:
			return (int64_t) u64;

		default:
			sign_mask = 1 << (type->realbits-1);
			value_mask = sign_mask - 1;
			/* Negative value: return 2-complement */
			if (u64 & sign_mask)
				return - ((~u64 & value_mask) + 1); 
			/* Positive value */
			else
				return (int64_t) u64;           
	}
}

float scale_value(int sensor_index, int channel, int64_t value) {
	float new_value;

	new_value = (float)value *
		g_sensor_info_iio_ext[sensor_index].channel_info[channel].opt_scale +
		g_sensor_info_iio_ext[sensor_index].offset;
		
	/* if scaled value is 0 we have a specific scale value for every channel */
	if (g_sensor_info_iio_ext[sensor_index].scale == 0) {
		new_value *= g_sensor_info_iio_ext[sensor_index].channel_info[channel].scale;
		return new_value;
	}
	new_value *= g_sensor_info_iio_ext[sensor_index].scale;
	return new_value;
}
