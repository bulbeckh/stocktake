#!/bin/bash

export GZ_SIM_RESOURCE_PATH=${GZ_SIM_RESOURCE_PATH}:$(pwd)/models:$(pwd)/models/meshes:
export GZ_SIM_SYSTEM_PLUGIN_PATH=${GZ_SIM_SYSTEM_PLUGIN_PATH}:$(pwd)/build:
