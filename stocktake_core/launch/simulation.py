### Launchfile for simulation start

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription,
                            OpaqueFunction, RegisterEventHandler)
from launch.conditions import IfCondition, UnlessCondition
from launch.event_handlers import OnShutdown
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml

def generate_launch_description() -> LaunchDescription:

    stocktake_core_dir = get_package_share_directory('stocktake_core')

    robot_type = LaunchConfiguration('robot_type')
    use_client = LaunchConfiguration('use_client')

    declare_robot_type_cmd = DeclareLaunchArgument(
        'robot_type',
        default_value='lidar',
        description='robot_type: lidar, rgbd, stereo, mono'
    )

    declare_use_client_cmd = DeclareLaunchArgument(
        'use_client',
        default_value='true',
        description='Whether we launch the gazebo client (as opposed to just the server)'
    )

    ## TODO Add launch-time procedural generation of world file (based on seed)
    generate_world_cmd = ExecuteProcess(
        cmd=['python3',
             os.path.join(stocktake_core_dir, 'worlds', 'generate', 'generate_store.py'),
             LaunchConfiguration('robot_type'),
             os.path.join(stocktake_core_dir, 'worlds', 'generate', 'out_generated.sdf'),
             'randomseed',
        ],
        output='screen'
    )

    gazebo_server = ExecuteProcess(
        cmd=['gz', 'sim', '-r', '-s', os.path.join(stocktake_core_dir, 'worlds', 'default.sdf')],
        output='screen',
    )

    gazebo_client = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('ros_gz_sim'),
                         'launch',
                         'gz_sim.launch.py')
        ),
        launch_arguments={'gz_args': ['-v4 -g ']}.items(),
        condition=IfCondition(use_client),
    )

    ## x. ROS-GZ Bridges
    lidar_bridge_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'lidar' "])),
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

    cmd_vel_bridge_cmd = Node(
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

    odom_bridge_cmd = Node(
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

    odom_tf_bridge_cmd = Node(
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

    clock_bridge_cmd = Node(
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

    ## Camera bridges
    '''We run a sub-set of these bridges, dependent on our camera array. Stella-vslam expects images published on the following
    topics:

    Mono: /camera/image_raw
    Stereo: /camera/left/image_raw and /camera/right/image_raw
    RGB-D: /camera/color/image_raw and /camera/depth/image_raw

    Additionally, we publish the camera_info to TODO'''

    ### RGB-D bridges
    rgbd_color_bridge_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'rgbd' "])),
            package="ros_gz_image",
            executable="image_bridge",
            name="rgbd_color_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/image'
            ],
            parameters=[{"use_sim_time": True}],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/image",
                    "/camera/color/image_raw",
                ),
            ],
    )

    rgbd_depth_bridge_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'rgbd' "])),
            package="ros_gz_image",
            executable="image_bridge",
            name="rgbd_depth_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/depth_image'
            ],
            parameters=[{"use_sim_time": True}],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/depth_image",
                    "/camera/depth/image_raw",
                ),
            ],
    )

    rgbd_pointcloud_bridge_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'rgbd' "])),
            package='ros_gz_bridge',
            executable='parameter_bridge',
            name='rgbd_pointcloud_bridge',
            output='screen',
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked',
            ],
            parameters=[
                {'use_sim_time': True}
            ],
            remappings=[
                ('/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/points',
                 '/camera/pointcloud'),
            ],
    )

    ### Stereo bridges
    stereo_left_bridge_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'stereo' "])),
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
                    "/camera/left/image_raw",
                ),
            ],
    )

    stereo_right_bridge_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'stereo' "])),
            package="ros_gz_image",
            executable="image_bridge",
            name="right_camera_image_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/right_camera/image'
            ],
            parameters=[{"use_sim_time": True}],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/right_camera/image",
                    "/camera/right/image_raw",
                ),
            ],
    )

    ## TODO In the stereo and mono bridges, we need to use something like 'depth_image_proc' to produce a pointcloud2
    ## from the images

    ### Mono bridges
    mono_bridge_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'mono' "])),
            package="ros_gz_image",
            executable="image_bridge",
            name="mono_image_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/image'
            ],
            parameters=[{"use_sim_time": True}],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/image",
                    "/camera/image_raw",
                ),
            ],
    )

    ## TODO In the stereo and mono bridges, we need to use something like 'depth_image_proc' to produce a pointcloud2
    ## from the images

    ld = LaunchDescription()

    ld.add_action(declare_robot_type_cmd)
    ld.add_action(declare_use_client_cmd)

    ld.add_action(gazebo_server)
    ld.add_action(gazebo_client)
    ld.add_action(generate_world_cmd)

    # Standard bridges
    ld.add_action(lidar_bridge_cmd)
    ld.add_action(clock_bridge_cmd)
    ld.add_action(odom_bridge_cmd)
    ld.add_action(odom_tf_bridge_cmd)
    ld.add_action(cmd_vel_bridge_cmd)

    # Camera bridges
    ld.add_action(rgbd_color_bridge_cmd)
    ld.add_action(rgbd_depth_bridge_cmd)
    ld.add_action(rgbd_pointcloud_bridge_cmd)
    ld.add_action(stereo_left_bridge_cmd)
    ld.add_action(stereo_right_bridge_cmd)
    ld.add_action(mono_bridge_cmd)

    return ld
