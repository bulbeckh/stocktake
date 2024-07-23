#!/bin/bash

#./util/mecanum_angle.py

## Call utilities
./util/layout.py > ./models/out.sdf

if [[ $? -eq 0 ]]; then
	echo "Generated layout.sdf"
else
	echo "[ERROR] Failed to generate layout.sdf"
	exit 0
fi

## Publish static frame transform
# NOTE Move this to ros2 launch file
#ros2 run tf2_ros static_transform_publisher 0.1 0 0.2 0 0 0 base_link base_laser

## Start ROS Nodes using ros2 launch
# ros2 launch xx xx

## Start simulation
gz sim -v4 main.sdf
