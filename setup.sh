#!/bin/bash

if [ -d "gazebo-rfid-plugin" ]; then
	## NOTE Currently, the models (rfid-scanner, rfid-tag) are not in a models/ sub-folder in gazebo-rfid-plugin 
	export GZ_SIM_RESOURCE_PATH=${GZ_SIM_RESOURCE_PATH}:$(pwd)/models:$(pwd)/gazebo-rfid-plugin:$(pwd):
	export GZ_SIM_SYSTEM_PLUGIN_PATH=${GZ_SIM_SYSTEM_PLUGIN_PATH}:$(pwd)/build:$(pwd)/build/gazebo-rfid-plugin:
	export GZ_DESCRIPTOR_PATH=${GZ_DESCRIPTOR_PATH}:$(pwd)/build:$(pwd)/build/gazebo-rfid-plugin:
else
	echo "Could not find gazebo-rfid-plugin/ folder. Have you run 'git submodule update --init --recursive'"
fi

