from launch import LaunchDescription
import os
from ament_index_python.packages import get_package_share_directory

from launch.actions import (DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription,
                            OpaqueFunction, RegisterEventHandler)
from launch.conditions import IfCondition
from launch.event_handlers import OnShutdown
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml

def generate_launch_description():
    world = LaunchConfiguration("world")
    use_sim_time = LaunchConfiguration('use_sim_time')

    declare_use_sim_time_cmd = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation (Gazebo) clock if true',
    )

    stocktake_core_dir = get_package_share_directory('stocktake_core')

    gazebo_server = ExecuteProcess(
        #cmd=['gz', 'sim', '-r', '-s', os.path.join(stocktake_core_dir, 'worlds/simplestore.sdf')],
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

    # Keyboard teleop in foreground
    teleop = ExecuteProcess(
        cmd=[
            "ros2", "run", "teleop_twist_keyboard",
            "teleop_twist_keyboard",
            "--ros-args",
            "-r", ["/cmd_vel:=", "/model/robotmodel/cmd_vel"]
        ],
        output="screen",
        emulate_tty=True
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
                {'use_sim_time': use_sim_time}
            ],
            remappings=[
                ('/model/robotmodel/cmd_vel', '/cmd_vel'),
            ]
    )

    ## Camera bridges
    ros_gz_camera_bridge1_cmd = Node(
            package="ros_gz_image",
            executable="image_bridge",
            name="left_camera_image_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/image'
            ],
            parameters=[{"use_sim_time": use_sim_time}],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/image",
                    "/camera/left/image_raw",
                ),
            ],
    )

    ros_gz_camera_bridge2_cmd = Node(
            package="ros_gz_image",
            executable="image_bridge",
            name="right_camera_image_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/right_camera/image'
            ],
            parameters=[{"use_sim_time": use_sim_time}],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/right_camera/image",
                    "/camera/right/image_raw",
                ),
            ],
    )

    ld = LaunchDescription()
    ld.add_action(declare_use_sim_time_cmd)

    ld.add_action(gazebo_server)
    ld.add_action(gazebo_client)
    #ld.add_action(teleop)
    ld.add_action(ros_gz_camera_bridge1_cmd)
    ld.add_action(ros_gz_camera_bridge2_cmd)
    ld.add_action(ros_gz_bridge3_cmd)

    return ld
