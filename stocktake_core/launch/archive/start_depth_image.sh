#!/bin/bash

ros2 run depth_image_proc point_cloud_xyzrgb_node \
  --log-level debug \
  --ros-args \
  -p use_sim_time:=true \
  -p queue_size:=10 \
  -p exact_sync:=false \
  -r /rgb/image_rect_color:=/camera/color/image_raw \
  -r /rgb/camera_info:=/camera/camera_info \
  -r /depth_registered/image_rect:=/camera/depth/image_raw
