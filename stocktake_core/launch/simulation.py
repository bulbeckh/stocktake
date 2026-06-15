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

'''This simulation launch file launches all of our simulation (gazebo) nodes, processes, and assets.

We first generate the world using the 'generate_store.py' script.

We then launch:

    - Gazebo Server
    - Gazebo Client (optional)

    - Command Velocity Bridge
    - Odom Bridge (publishes on /odom)
    - Odom TF Bridge (publishes odom -> base_link on /tf)
    - Clock Bridge
    - RFID Scan Service Bridge (custom ros <-> gz service bridge)

If we are using LiDAR robot:
    - LiDAR Bridge

If we are using RGB-D robot:
    - Camera Colour Image Bridge
    - Camera Depth Image Bridge
    - Camera Depth PointCloud Bridge

If we are using Stereo RGB robot:
    - Camera Colour Left Bridge
    - Camera Colour Right Bridge

If we are using Mono RGB robot:
    - Camera Colour Bridge

# TODO We have issues with this bypass_slam functionality
Lastly, if we are bypassing slam (during debug), we should be able to
retrieve robot pose (and compute map -> odom) directly. We launch:
    - Robot Pose Bridge
    - Pose TF Publisher'''

def generate_launch_description() -> LaunchDescription:

    stocktake_core_dir = get_package_share_directory('stocktake_core')

    robot_type = LaunchConfiguration('robot_type')
    bypass_slam = LaunchConfiguration('bypass_slam')
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

    declare_bypass_slam_cmd = DeclareLaunchArgument(
        'bypass_slam',
        default_value='true',
        description='Set to true to publish map -> odom transform using gazebo world pose rather than through a slam node (lidar or vslam). Used for testing/debug.'
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
        cmd=['gz', 'sim', '-v4', '-r', '-s', os.path.join(stocktake_core_dir, 'worlds', 'default.sdf')],
        output='screen',
    )

    gazebo_client = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('ros_gz_sim'), 'launch', 'gz_sim.launch.py')
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
                 'override_frame_id': 'robot_lidar'}
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
                ## NOTE Need to do this since we turned off collision monitor
                #('/model/robotmodel/cmd_vel', '/cmd_vel'),
                ('/model/robotmodel/cmd_vel', '/cmd_vel_smoothed'),
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
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="rgbd_color_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/image@sensor_msgs/msg/Image[gz.msgs.Image'
            ],
            parameters=[{
                "use_sim_time": True,
                "override_frame_id": "optical_camera_frame",
            }],
            remappings=[
                (
                    "/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/image",
                    "/camera/color/image_raw",
                ),
            ],
    )

    rgbd_depth_bridge_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'rgbd' "])),
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="rgbd_depth_bridge",
            output="screen",
            arguments=[
                '/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/camera/depth_image@sensor_msgs/msg/Image[gz.msgs.Image'
            ],
            parameters=[{
                "use_sim_time": True,
                "override_frame_id": "optical_camera_frame",
            }],
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
            parameters=[{
                'use_sim_time': True,
                'qos_overrides./points.publisher.reliability': 'best_effort',
                'qos_overrides./points.publisher.durability': 'volatile',
                'qos_overrides./points.publisher.history': 'keep_last',
                'qos_overrides./points.publisher.depth': 1,
                #"override_frame_id": "optical_camera_frame",
            }],
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

    ## Gazebo RFID Scan service bridge (custom)
    rfid_scan_service_bridge = Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="scan_service_bridge",
            output="screen",
            arguments=[
                '/rfid_scanner/scan_request@gazebo_rfid_plugin/srv/RFIDScan@gz.msgs.Empty@gz.custom_msgs.RFIDScanResponse'
                ],
            parameters=[{"use_sim_time": True}],
    )

    ## SLAM Bypasses (publishing of robot pose in gz map frame as map -> odom tf)
    robot_pose_tf_publisher = Node(
            condition=IfCondition(LaunchConfiguration('bypass_slam')),
            package='robot_pose_pub',
            executable='model_pose_to_map_odom',
            name='robot_pose_tf_publisher',
            output='screen',
            parameters=[{
                'input_topic': '/model_pose',
                'keep_every_n': 1,
                'robot_base_frame_id': 'robot_base',
                'x_offset': -10.0,
                'y_offset': -1.0,
            }]
    )

    robot_pose_bridge = Node(
            condition=IfCondition(LaunchConfiguration('bypass_slam')),
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="robot_pose_bridge",
            output="screen",
            arguments=[
                '/model/othermodel/model/robotmodel/pose@geometry_msgs/msg/TransformStamped[gz.msgs.Pose'
            ],
            parameters=[{
                "use_sim_time": True,
            }],
            remappings=[
                (
                    "/model/othermodel/model/robotmodel/pose",
                    "/model_pose"
                ),
            ],
    )

    ld = LaunchDescription()

    ld.add_action(declare_robot_type_cmd)
    ld.add_action(declare_use_client_cmd)
    ld.add_action(declare_bypass_slam_cmd)

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

    # Robot pose bridges
    ld.add_action(robot_pose_tf_publisher)
    ld.add_action(robot_pose_bridge)

    ## Scan service bridge
    ld.add_action(rfid_scan_service_bridge)

    return ld
