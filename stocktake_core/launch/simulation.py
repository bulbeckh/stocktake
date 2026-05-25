### Launchfile for simulation start

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription,
                            OpaqueFunction, RegisterEventHandler)
from launch.conditions import IfCondition
from launch.event_handlers import OnShutdown
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml

def generate_launch_description() -> LaunchDescription:

    stocktake_core_dir = get_package_share_directory('stocktake_core')

    ## TODO Add launch-time procedural generation of world file (based on seed)

    ## TODO Update world_sdf with path to sdf
    gazebo_server = ExecuteProcess(
        cmd=['gz', 'sim', '-r', '-s', os.path.join(stocktake_core_dir, 'worlds/default.sdf')],
        output='screen',
    )

    gazebo_client = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('ros_gz_sim'),
                         'launch',
                         'gz_sim.launch.py')
        ),
        launch_arguments={'gz_args': ['-v4 -g ']}.items(),
    )

    ## x. ROS-GZ Bridges
    ros_gz_bridge_cmd = Node(
            package='ros_gz_bridge',
            executable='parameter_bridge',
            name='lidar_bridge',
            output='screen',
            arguments=[
                '/lidar@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan',
            ],
            parameters=[
                {'use_sim_time': True,
                 'expand_gz_topic_names': True,
                 }
            ],
            remappings=[
                ('/lidar', '/scan'),
            ],
    )

    ros_gz_bridge2_cmd = Node(
            package='ros_gz_bridge',
            executable='parameter_bridge',
            name='odom_bridge',
            output='screen',
            arguments=[
                '/model/robotmodel/odometry@nav_msgs/msg/Odometry[gz.msgs.Odometry'
            ],
            parameters=[
                {'use_sim_time': True}
            ],
            remappings=[
                ('/model/robotmodel/odometry', '/odom'),
            ]
    )

    ros_gz_bridge3_cmd = Node(
            package='ros_gz_bridge',
            executable='parameter_bridge',
            name='cmd_vel_bridge',
            output='screen',
            arguments=[
                '/model/robotmodel/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist'
            ],
            parameters=[
                {'use_sim_time': True}
            ],
            remappings=[
                ('/model/robotmodel/cmd_vel', '/cmd_vel'),
            ]
    )

    ros_gz_bridge4_cmd = Node(
            package='ros_gz_bridge',
            executable='parameter_bridge',
            name='odom_tf_bridge',
            output='screen',
            arguments=[
                '/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V'
            ],
            parameters=[
                {'use_sim_time': True}
            ],
    )

    ros_gz_bridge5_cmd = Node(
            package='ros_gz_bridge',
            executable='parameter_bridge',
            name='clock_bridge',
            output='screen',
            arguments=[
                '/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock'
            ],
            parameters=[
                {'use_sim_time': True}
            ],
    )

    ### RGB-D bridges
    ros_gz_camera_bridge1_cmd = Node(
            package="ros_gz_image",
            executable="image_bridge",
            name="left_camera_image_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/image'
            ],
            parameters=[{"use_sim_time": True}],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/image",
                    "/camera/color/image_raw",
                ),
            ],
    )

    ros_gz_camera_bridge2_cmd = Node(
            package="ros_gz_image",
            executable="image_bridge",
            name="right_camera_image_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/depth_image'
            ],
            parameters=[{"use_sim_time": True}],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/depth_image",
                    "/camera/depth/image_raw",
                ),
            ],
    )

    ros_gz_camera_bridge3_cmd = Node(
            package="ros_gz_image",
            executable="image_bridge",
            name="right_camera_image_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/camera_info'
            ],
            parameters=[{"use_sim_time": True}],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/camera_info",
                    "/camera/camera_info",
                ),
            ],
    )

    ros_gz_bridge6_cmd = Node(
            package='ros_gz_bridge',
            executable='parameter_bridge',
            name='pointcloud_bridge',
            output='screen',
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked',
            ],
            parameters=[
                {'use_sim_time': True}
            ],
            remappings=[
                ('/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/points', '/camera/pointcloud'),
            ],
    )

    ld = LaunchDescription()

    ld.add_action(gazebo_server)
    ld.add_action(gazebo_client)

    ld.add_action(ros_gz_bridge_cmd)
    ld.add_action(ros_gz_bridge2_cmd)
    ld.add_action(ros_gz_bridge3_cmd)
    ld.add_action(ros_gz_bridge4_cmd)
    ld.add_action(ros_gz_bridge5_cmd)

    ld.add_action(ros_gz_camera_bridge1_cmd)
    ld.add_action(ros_gz_camera_bridge2_cmd)
    ld.add_action(ros_gz_camera_bridge3_cmd)
    ld.add_action(ros_gz_bridge6_cmd)

    return ld
