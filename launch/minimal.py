from launch import LaunchDescription
from launch.actions import ExecuteProcess, DeclareLaunchArgument
from launch_ros.actions import Node

import os

def generate_launch_description():
  root_dir = os.path.dirname(os.path.realpath(__file__))
  print(f'Root DIR: {root_dir}')

  return LaunchDescription([
    ExecuteProcess(cmd=['gz','sim','v1','-r', 'main.sdf'], output='screen'),
    Node(
					package='ros_gz_bridge',
					executable='parameter_bridge',
					arguments=["/imu@sensor_msgs/msg/Imu[gz.msgs.IMU",
												"/lidar@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan",
												"/model/robotmodel/odometry@nav_msgs/msg/Odometry[gz.msgs.Odometry",
												"/model/robotmodel/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V"
		]),
    Node(
          package='tf2_ros',
          executable='static_transform_publisher',
          arguments=['0', '0', '0.05', '0', '0', '0', 'base_link', 'robotmodel/robot-lidar/robot-lidar']
    ),
    Node(
          package='tf2_ros',
          executable='static_transform_publisher',
          arguments=['0', '0', '0.1', '0', '0', '0', 'base_link', 'robotmodel/imu/imu-sensor']
    ),
    Node(
          package='tf2_ros',
          executable='static_transform_publisher',
          arguments=['0', '0', '0', '0', '0', '0', 'base_link', 'robotmodel/odom']
    ),
  ])

'''
    Node(
					package='robot_localization',
					executable='ekf_node',
					namespace='localization',
					name='ekf_filter_node',
					output='screen',
					parameters=[os.path.join(root_dir, '../config/ekf.yaml')]
    ),
    Node(
					package='rviz2',
					executable='rviz2'
    )
				Node(
					package='slam_toolbox',
					executable='async_slam_toolbox_node',
					parameters=[{'slam_params_file': os.path.join(root_dir, '../config/costmap.yaml')}]
				),

    Node(
          package='slam_toolbox',
          executable='async_slam_toolbox_node',
          name='slam_toolbox',
          output='screen',
          parameters=[os.path.join(root_dir, '../config.costmap.yaml')]
    ),
    Node(
					package='robot_localization',
					executable='ekf_node',
					name='ekf_filter_node',
					output='screen',
					parameters=[os.path.join(root_dir, '../config/ekf.yaml')]
    ),
'''
