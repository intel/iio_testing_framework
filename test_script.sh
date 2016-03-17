# Copyright (c) 2015 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#!/bin/sh
while getopts ":l:e:r:o:d:p:s:c:" opt; do
	case "$opt" in
		l) log_level=$OPTARG ;;
		e) executable_path=$OPTARG ;;
		r) results_path=$OPTARG ;;
		o) output_path=$OPTARG ;;
		d) devices=$OPTARG ;;
		p) devices_info_path=$OPTARG ;;
		s) suite_path=$OPTARG ;;
		c) cmd_line=$OPTARG ;;
		:) echo "No argument for $OPTARG!" ;;
		?) echo "No option for $OPTARG defined!" ;;
	esac
done

if [ -z "$devices" ]; then
	devices=$(cat $devices_info_path | cut -f1)
fi

for dev in $devices; do
	serial=$(cat $devices_info_path | grep -w $dev | rev | cut -f1 | rev)
	adb -s $serial root
	sleep 2
	adb -s $serial remount
	adb -s $serial shell dumpsys input_method | grep mInteractive=true 
	if [ $(echo $?) -eq 0 ]; then
		adb -s $serial shell input keyevent 26
	fi	
	adb -s $serial shell stop
	adb -s $serial push $executable_path system/bin/iio_testing_framework  
	for suit in $suite_path; do
		suit_name="${suit%/*}"
		suit_no_ext="${suit_name%.*}"
		timestamp=$(date +%s)
		path="$results_path/$timestamp/$dev/$suit_no_ext"
		adb -s $serial shell mkdir -p $path 
		adb -s $serial shell mkdir -p $path/logs
		if [ "$suit_name" != "cmdLine" ]; then
			adb -s $serial push $suit $path/$suit_name
			adb -s $serial shell /system/bin/iio_testing_framework -l $log_level -s $path/$suit_name -p $path
			
		else
			adb -s $serial shell /system/bin/iio_testing_framework -l $log_level -s $path/$suit_name -p $path -c "$cmd_line"
		fi
	done
	adb -s $serial pull $results_path $output_path
	adb -s $serial shell rm system/bin/iio_testing_framework
	adb -s $serial shell rm -r $results_path
	adb -s $serial shell start
done