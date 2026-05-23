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

"""
This launch file creates the core elements of the stocktake system, including:

- Gazebo simulation (including optional client)
- Static transforms 


"""

def generate_launch_description() -> LaunchDescription:
    # Get the launch directory

    bringup_dir = get_package_share_directory('nav2_bringup')
    launch_dir = os.path.join(bringup_dir, 'launch')

    stocktake_core_dir = get_package_share_directory('stocktake_core')

    # Create the launch configuration variables
    namespace = LaunchConfiguration('namespace')
    map_yaml_file = LaunchConfiguration('map')
    graph_filepath = LaunchConfiguration('graph')

    use_sim_time = LaunchConfiguration('use_sim_time')

    params_file = LaunchConfiguration('params_file')
    slam_params_file = LaunchConfiguration('slam_params_file')

    use_composition = LaunchConfiguration('use_composition')
    use_intra_process_comms = LaunchConfiguration('use_intra_process_comms')
    use_respawn = LaunchConfiguration('use_respawn')

    # Launch configuration variables specific to simulation
    rviz_config_file = LaunchConfiguration('rviz_config_file')

    # Declare the launch arguments
    declare_namespace_cmd = DeclareLaunchArgument(
        'namespace', default_value='', description='Top-level namespace'
    )


    declare_map_yaml_cmd = DeclareLaunchArgument(
        'map',
        default_value=os.path.join(bringup_dir, 'maps', 'tb3_sandbox.yaml'),
    )

    declare_graph_file_cmd = DeclareLaunchArgument(
        'graph',
        default_value=os.path.join(bringup_dir, 'graphs', 'turtlebot3_graph.geojson'),
    )

    declare_use_sim_time_cmd = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation (Gazebo) clock if true',
    )

    declare_params_file_cmd = DeclareLaunchArgument(
        'params_file',
        default_value=os.path.join(bringup_dir, 'params', 'nav2_params.yaml'),
        description='Full path to the ROS2 parameters file to use for all launched nodes',
    )

    declare_slam_params_file_cmd = DeclareLaunchArgument(
        'slam_params_file',
        default_value='/opt/ros/jazzy/share/slam_toolbox/config/mapper_params_online_sync.yaml',
        description='Full path to the ROS2 SLAM parameters',
    )


    declare_use_composition_cmd = DeclareLaunchArgument(
        'use_composition',
        default_value='False',
        description='Whether to use composed bringup',
    )

    declare_use_intra_process_comms_cmd = DeclareLaunchArgument(
        'use_intra_process_comms',
        default_value='False',
        description='Whether to use intra process communication',
    )

    declare_use_respawn_cmd = DeclareLaunchArgument(
        'use_respawn',
        default_value='False',
        description='Whether to respawn if a node crashes. Applied when composition is disabled.',
    )

    declare_rviz_config_file_cmd = DeclareLaunchArgument(
        'rviz_config_file',
        default_value=os.path.join(bringup_dir, 'rviz', 'nav2_default_view.rviz'),
        description='Full path to the RVIZ config file to use',
    )



    
    ## Re-configure certain configuration files
    slam_params_configured = RewrittenYaml(
        source_file=slam_params_file,
        root_key=namespace,
        param_rewrites={
            'slam_toolbox.ros__parameters.base_frame': 'robot_base',
            'slam_toolbox.ros__parameters.odom_frame': 'odom',
            'slam_toolbox.ros__parameters.map_frame': 'map',
            'slam_toolbox.ros__parameters.scan_topic': '/scan',
            'slam_toolbox.ros__parameters.max_laser_range': '10.0',
            'slam_toolbox.ros__parameters.minimum_travel_distance': '0.1',
            'slam_toolbox.ros__parameters.minimum_travel_heading': '0.1',
            'slam_toolbox.ros__parameters.debug_logging': 'True',
        },
        convert_types=True,
    )

    configured_params = RewrittenYaml(
        source_file=params_file,
        root_key=namespace,
        param_rewrites={
            'global_costmap.global_costmap.ros__parameters.origin_x': '-25.0',
            'global_costmap.global_costmap.ros__parameters.origin_y': '-25.0',
            'global_costmap.global_costmap.ros__parameters.obstacle_layer.scan.sensor_frame': 'store_layout/robotmodel/robot_lidar/robot_lidar',
            'local_costmap.local_costmap.ros__parameters.voxel_layer.scan.sensor_frame': 'store_layout/robotmodel/robot_lidar/robot_lidar',
            'collision_monitor.ros__parameters.scan.topic': '/scan',
            'local_costmap.local_costmap.ros__parameters.inflation_layer.inflation_radius': '0.4',
            'global_costmap.global_costmap.ros__parameters.inflation_layer.inflation_radius': '0.4',
        },
        convert_types=True,
    )

    ## Launch Rviz2 with Nav2 Rviz2 configuration
    rviz_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(launch_dir, 'rviz_launch.py')),
        launch_arguments={
            'namespace': namespace,
            'use_sim_time': use_sim_time,
            'rviz_config': rviz_config_file,
        }.items(),
    )

    ## Launch Nav2 bringup
    bringup_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(launch_dir, 'bringup_launch.py')),
        launch_arguments={
            'namespace': namespace,
            'slam': 'True',
            'map': map_yaml_file,
            'graph': graph_filepath,
            'use_sim_time': use_sim_time,
            'params_file': configured_params, ## params_file,
            'slam_params_file': slam_params_configured,
            'autostart': 'True',
            'use_composition': use_composition,
            'use_intra_process_comms': use_intra_process_comms,
            'use_respawn': use_respawn,
            'use_keepout_zones': 'False',
            'use_speed_zones': 'False',
            'container_name': 'nav2_container',
        }.items(),
    )

    static_transform_cmd = Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_publisher1',
            namespace='',
            output='screen',
            arguments=[
                '--frame-id',
                'robot_lidar',
                '--child-frame-id',
                'store_layout/robotmodel/robot_lidar/robot_lidar'
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
            ],
    )

    static_transform2_cmd = Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_publisher2',
            namespace='',
            output='screen',
            arguments=[
                '--frame-id',
                'robot_base',
                '--child-frame-id',
                'robot_lidar',
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
            ],
    )

    static_transform3_cmd = Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_publisher3',
            namespace='',
            output='screen',
            arguments=[
                '--frame-id',
                'base_link',
                '--child-frame-id',
                'robot_base'
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
            ],
    )

    ## TODO We should be changing the frame parameters in nav2 stack rather than have static bridge - need to change
    static_transform4_cmd = Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_publisher4',
            namespace='',
            output='screen',
            arguments=[
                '--frame-id',
                'base_link',
                '--child-frame-id',
                'base_footprint'
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
            ],
    )

    static_transform5_cmd = Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_publisher5',
            namespace='',
            output='screen',
            arguments=[
                '--frame-id',
                'store_layout/robotmodel/robot_lidar/robot_lidar',
                '--child-frame-id',
                'store_layout/robotmodel/camera_front/left_camera'
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
            ],
    )


    ## TODO Update world_sdf with path to sdf
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
                {'use_sim_time': use_sim_time,
                 'expand_gz_topic_names': True,
                 }
            ],
            remappings=[
                # Optional: remap gz topic to ROS topic
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
                {'use_sim_time': use_sim_time}
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
                {'use_sim_time': use_sim_time}
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
                {'use_sim_time': use_sim_time}
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
                {'use_sim_time': use_sim_time}
            ],
    )

    ## Camera bridges

    ### Left and right bridges
    '''
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
    '''

    ### RGB-D bridges
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
            parameters=[{"use_sim_time": use_sim_time}],
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
            parameters=[{"use_sim_time": use_sim_time}],
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
                {'use_sim_time': use_sim_time}
            ],
            remappings=[
                ('/world/default/model/store_layout/model/robotmodel/link/camera_front/sensor/left_camera/points', '/camera/pointcloud'),
            ],
    )

    ## Octomap nodes
    # NOTE occupancy_{min,max}_z is used to generate the 2D occupancy grid projection - need to decide on range to use
    octomap_node_cmd = Node(
            package='octomap_server',
            executable='octomap_server_node',
            name='octomap_server',
            #output='screen',
            remappings=[('cloud_in', '/camera/pointcloud')],
            parameters=[{
                'occupancy_max_z': 1.0,
                'occupancy_min_z': -1.0,
            }],
    )

    # TODO Either fix the LD_PRELOAD workaround (via patch) or retrieve lib directory differentely
    octomap_rviz_cmd = Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            output='screen',
            arguments=['-d', '/workspaces/stocktake-alt/src/stocktake/config/octomap.rviz'],
            additional_env={
                'LD_PRELOAD': '/opt/ros/jazzy/lib/x86_64-linux-gnu/liboctomap.so'
            }
    )

    # Create the launch description and populate
    ld = LaunchDescription()

    # Declare the launch options
    ld.add_action(declare_namespace_cmd)
    ld.add_action(declare_map_yaml_cmd)
    ld.add_action(declare_graph_file_cmd)
    ld.add_action(declare_use_sim_time_cmd)
    ld.add_action(declare_params_file_cmd)
    ld.add_action(declare_slam_params_file_cmd)
    ld.add_action(declare_use_composition_cmd)
    ld.add_action(declare_use_intra_process_comms_cmd)

    ld.add_action(declare_rviz_config_file_cmd)
    ld.add_action(declare_use_respawn_cmd)

    ld.add_action(gazebo_server)
    ld.add_action(gazebo_client)

    ld.add_action(ros_gz_bridge_cmd)
    ld.add_action(ros_gz_bridge2_cmd)
    ld.add_action(ros_gz_bridge3_cmd)
    ld.add_action(ros_gz_bridge4_cmd)
    ld.add_action(ros_gz_bridge5_cmd)

    ld.add_action(ros_gz_camera_bridge1_cmd)
    ld.add_action(ros_gz_camera_bridge2_cmd)
    ld.add_action(ros_gz_bridge6_cmd)

    ld.add_action(static_transform_cmd)
    ld.add_action(static_transform2_cmd)
    ld.add_action(static_transform3_cmd)
    ld.add_action(static_transform4_cmd)
    ld.add_action(static_transform5_cmd)

    ld.add_action(rviz_cmd)
    ld.add_action(bringup_cmd)

    ld.add_action(octomap_node_cmd)
    ld.add_action(octomap_rviz_cmd)

    return ld
