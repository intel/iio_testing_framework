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

#ifndef __IIO_COMMON_H__
#define __IIO_COMMON_H__
#define MAX_DEVICES	9	/* Check iio devices 0 to MAX_DEVICES-1 */
#define MAX_SENSORS	12	/* We can handle as many sensors */
#define MAX_CHANNELS	4	/* We can handle as many channels per sensor */
#define MAX_TRIGGERS	8	/* Check for triggers 0 to MAX_TRIGGERS-1 */

#define DEV_FILE_PATH		"/dev/iio:device%d"
#define BASE_PATH		"/sys/bus/iio/devices/iio:device%d/"
#define TRIGGER_FILE_PATH	"/sys/bus/iio/devices/trigger%d/name"
#define TRIGGER_FREQ_PATH	"/sys/bus/iio/devices/trigger%d/sampling_frequency"
#define PLD_PANEL_PATH	BASE_PATH "../firmware_node/pld/panel"
#define PLD_ROTATION_PATH	BASE_PATH "../firmware_node/pld/rotation"
#define CHANNEL_PATH		BASE_PATH "scan_elements/"
#define ENABLE_PATH		BASE_PATH "buffer/enable"
#define NAME_PATH		BASE_PATH "name"
#define TRIGGER_PATH		BASE_PATH "trigger/current_trigger"
#define SENSOR_ENABLE_PATH	BASE_PATH "in_%s_en"
#define SENSOR_OFFSET_PATH	BASE_PATH "in_%s_offset"
#define SENSOR_SCALE_PATH	BASE_PATH "in_%s_scale"
#define SENSOR_SAMPLING_PATH	BASE_PATH "in_%s_sampling_frequency"
#define DEVICE_SAMPLING_PATH	BASE_PATH "sampling_frequency"
#define DEVICE_AVAIL_FREQ_PATH	BASE_PATH "sampling_frequency_available"
#define BUFFER_PATH		BASE_PATH "buffer"
#define SCAN_ELEMENTS_PATH		BASE_PATH "scan_elements"
#define CONFIGFS_TRIGGER_PATH	"/sys/kernel/config/iio/triggers/"
#define TIMESTAMP_ENABLE_PATH	CHANNEL_PATH "in_timestamp_en"
#define TIMESTAMP_TYPE_PATH	CHANNEL_PATH "in_timestamp_type"
#define TIMESTAMP_INDEX_PATH	CHANNEL_PATH "in_timestamp_index"
#define TESTS_MSG	"/tests_msg"	
#define TESTS_RESULTS	"/tests_results"
#define TESTS_LOGS     "/logs/test_"
#define DATA_READY_SIGNAL	"R" 

#define MAX_JITTER	3
#define COUNTER 3000 
#define PATH_MAX 4096
#define BUFFER_SIZE	512
#define TIME_SIZE	64
#define MAX_DELAY	500000000 /* 500 ms */ 
#define NUMTESTS	40
#define TIME_TO_MEASURE_SECS	20
#define TIME_TO_MEASURE_MILLISECS	20000 
#define CONVERT_SEC_TO_NANO(x)	((x) * 1000000000)
#define CONVERT_SEC_TO_MICRO(x)	((x) * 1000000)
#define CONVERT_SEC_TO_MILLI(x)	((x) * 1000)
#define CONVERT_NANO_TO_MILLI(x)	((x)/1000000)
#define CONVERT_MILLI_TO_SEC(x)	((x)/1000)
#define CONVERT_MICROTESLA_TO_GAUSS(x)	((x)/100) 
#define ARRAY_SIZE(x) sizeof(x)/sizeof(x[0])

#define SENSOR_TYPE_ACCELEROMETER	0
#define SENSOR_TYPE_GYROSCOPE	1
#define SENSOR_TYPE_MAGNETIC_FIELD	2
#define SENSOR_TYPE_INTERNAL_INTENSITY	3
#define SENSOR_TYPE_INTERNAL_ILLUMINANCE	4
#define SENSOR_TYPE_ORIENTATION	5
#define SENSOR_TYPE_ROTATION_VECTOR	6
#define SENSOR_TYPE_AMBIENT_TEMPERATURE	7
#define SENSOR_TYPE_PROXIMITY	8
/* Channel type spec len; ex: "le:u10/16>>0" */
#define MAX_TYPE_SPEC_LEN	32	
#define MAX_NAME_SIZE		32

#define MODE_AUTO	0 /* autodetect */
#define MODE_POLL	1
#define MODE_TRIGGER	2
#define MODE_EVENT	3

#define PANEL_FRONT	4
#define PANEL_BACK	5

/* If buffer enable fails, we may want 
** to retry a few times before giving up 
*/
#define ENABLE_BUFFER_RETRIES		3
#define ENABLE_BUFFER_RETRY_DELAY_MS	10
#define INF	99999999
#define MEASURE_FREQ 1
#define CHECK_SAMPLE_TIMESTAMP_AVG_DIFF 2
#define CHECK_SAMPLE_TIMESTAMP_DIFF 3
#define CHECK_CLIENT_AVG_DELAY	4
#define CHECK_CLIENT_DELAY	5
#define JITTER	6
#define STANDARD_DEVIATION	7

#define HASHMAP_SIZE    15

#define PROCESS 1
#define FINALIZE 0

#define DESCRIPTION 	0
#define CONTENT		1

#define READ 0
#define WRITE 1

/* define log levels for messages
** written in tests results
*/
typedef enum level_t{
	NOTHING	= 0,
	FATAL = 1,
	ERROR = 2,
	DEBUG = 3,
	VERBOSE	= 4
}level;

/* use tags from tests to set a parsing state 
** in order to separate values 
*/
typedef enum parsing_state_t{
	INIT_STATE	= 0,
	SENSOR_TAG_STATE = 1,
	FREQ_STATE = 2,
	DELAY_STATE = 3,
	DURATION_STATE = 4,
	COUNTER_STATE = 5,
	FINISH_STATE = 6
}parsing_state;

/* define tests states 
** skipped is for sensors that
** 		   don't support a certain test 
*/
typedef enum test_state_t{
	PASSED = 1,
	FAILED = -1,
	SKIPPED = 0 
}test_state;

/* define attributes for each test
** frequency is rate on which sensors are set for tests 
** max_delay is maximum error accepted between measured
** 			 delay and set delay by each test
*/ 
typedef struct time_attributes_struct_t{
	int max_delay;
	float freq;
}time_attributes_struct;

/* define tests structure*/
typedef struct
{
	char *description;
	int log_fd;
	test_state state;
}
test_info_t;

/* define structure for frequency and timestamp tests 
** counter is nr of collected values 
*/
typedef struct timestamp_info_struct_t{
	int counter;
	int64_t all_consec_timestamps_diff;
}timestamp_info_struct;

/* define structure for jitter tests */
typedef struct jitter_struct_t{
	int counter;
	int timestamp_values_size;
	int64_t *timestamp_values;
}jitter_struct;

/* define structure for standard deviation */
typedef struct standard_deviation_struct_t{
	int counter;
	int channels_values_size;
	float **channels_values;
}standard_deviation_struct;

typedef struct
{
	const char *name;	/* channel name ; ex: x */

	/* sysfs entries located under scan_elements */
	const char *en_path;	/* Enabled sysfs file name ; ex: "in_temp_en" */
	const char *type_path;	/* _type sysfs file name  */
	const char *index_path;	/* _index sysfs file name */
	
	/* sysfs entries located in /sys/bus/iio/devices/iio:deviceX */
	const char *raw_path;	/* _raw sysfs file name  */
	const char *input_path;	/* _input sysfs file name */
	const char *scale_path;	/* _scale sysfs file name */
}
channel_descriptor_t;

typedef struct
{
	char sign;
	char endianness;
	short realbits;
	short storagebits;
	short shift;
}
datum_info_t;

typedef struct
{
	int size;	/* Field size in bytes */
	int index;	/*associated index*/
	float last_value;
	int opt_scale;
	float scale;	/* Scale for each channel */
	char type_spec[MAX_TYPE_SPEC_LEN];	/* From driver; ex: le:u10/16>>0 */
	datum_info_t type_info;	   		/* Decoded contents of type spec */

}
channel_info_t;

typedef struct {
	char internal_name[MAX_NAME_SIZE];	/* ex: accel_3d	             */
	char init_trigger_name[MAX_NAME_SIZE];	/* ex: accel-name-dev1	     */
	const int type;		/* Sensor type ; ex: SENSOR_TYPE_ACCELEROMETER */
	char triggers[MAX_TRIGGERS][MAX_NAME_SIZE];
	int trigger_nr;	/* number of triggers associated with this device */
	int found_implicit_trigger;
	int hr_trigger_nr;
	float offset;		/* (cooked = raw + offset) * scale			*/
	float scale;		/* default:1. when set to 0, use channel specific value */
	int dev_num;		/* Associated iio dev num, ex: 3 for /dev/iio:device3	*/
	int num_channels;	/* Actual channel count ; 0 for poll mode sensors	*/
	int mode;	/* Usage mode, ex: poll, trigger ... */
	int is_virtual;
	channel_info_t channel_info[MAX_CHANNELS];
	channel_info_t timestamp;
	const char *tag;	/* Prefix such as "accel", "gyro", "temp"... */
	channel_descriptor_t channel_descriptor[MAX_CHANNELS];
	int read_fd;
	int write_fd;
	int64_t last_timestamp;
	float data_rate;
	int discovered;
	int sample_size;
} sensor_info_iio_ext_t;



/*
** Macros associating iio sysfs entries to sensor types 
*/

#define DECLARE_VOID_CHANNEL(tag)	\
			tag,	\
			"",	\
			"",	\
			"",	\
			"",	\
			"",	\
			"",	\

#define DECLARE_CHANNEL(tag, spacer, name)		\
			name,				\
			"in_" tag spacer name "_en",	\
			"in_" tag spacer name "_type",	\
			"in_" tag spacer name "_index",	\
			"in_" tag spacer name "_raw",	\
			"in_" tag spacer name "_input",	\
			"in_" tag spacer name "_scale",	\

#define DECLARE_NAMED_CHANNEL(tag, name)	DECLARE_CHANNEL(tag, "_", name)

#define DECLARE_GENERIC_CHANNEL(tag)		DECLARE_CHANNEL(tag, "", "")

extern test_info_t *tests;
extern sensor_info_iio_ext_t g_sensor_info_iio_ext[MAX_SENSORS];
extern int g_sensor_info_size;
extern int g_sensor_iio_count;
extern int current_fd;
extern int nr_test;
extern level log_level;
extern Hashmap *map_sensor_index_to_time_attributes;
extern Hashmap *map_fd_to_sensor_index;
#endif
