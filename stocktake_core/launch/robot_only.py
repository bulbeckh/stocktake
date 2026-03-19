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
    # Get the launch directory

    bringup_dir = get_package_share_directory('nav2_bringup')
    launch_dir = os.path.join(bringup_dir, 'launch')

    # Create the launch configuration variables
    namespace = LaunchConfiguration('namespace')
    use_sim_time = LaunchConfiguration('use_sim_time')

    # Launch configuration variables specific to simulation
    rviz_config_file = LaunchConfiguration('rviz_config_file')
    use_robot_state_pub = LaunchConfiguration('use_robot_state_pub')
    use_rviz = LaunchConfiguration('use_rviz')

    remappings = [('/tf', 'tf'), ('/tf_static', 'tf_static')]

    # Declare the launch arguments
    declare_namespace_cmd = DeclareLaunchArgument(
        'namespace', default_value='', description='Top-level namespace'
    )

    declare_use_sim_time_cmd = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation (Gazebo) clock if true',
    )

    declare_rviz_config_file_cmd = DeclareLaunchArgument(
        'rviz_config_file',
        default_value='../config/rviz_robot_only.rviz',
        description='Full path to the RVIZ config file to use',
    )

    declare_use_robot_state_pub_cmd = DeclareLaunchArgument(
        'use_robot_state_pub',
        default_value='True',
        description='Whether to start the robot state publisher',
    )

    declare_use_rviz_cmd = DeclareLaunchArgument(
        'use_rviz', default_value='True', description='Whether to start RVIZ'
    )
    

    with open('../models/robot/robot.sdf', 'r') as infp:
        robot_description = infp.read()

    start_robot_state_publisher_cmd = Node(
        condition=IfCondition(use_robot_state_pub),
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        namespace=namespace,
        output='screen',
        parameters=[
            {'use_sim_time': use_sim_time, 'robot_description': robot_description }
        ],
        remappings=remappings,
    )

    rviz_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(launch_dir, 'rviz_launch.py')),
        launch_arguments={
            'namespace': namespace,
            'use_sim_time': use_sim_time,
            'rviz_config': rviz_config_file,
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
                'robot-lidar',
                '--child-frame-id',
                'robotmodel/robot-lidar/robot-lidar'
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
                'robot-base'
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
            ],
    )

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

    gazebo_server = ExecuteProcess(
        cmd=['gz', 'sim', '-r', '-s', '../worlds/robot_only.sdf'],
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
                '--ros-args',
                '-p',
                'override_frame_id:=robot-lidar'
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
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

    ## Camera bridge
    ros_gz_camera_bridge_cmd = Node(
            package="ros_gz_image",
            executable="image_bridge",
            name="camera_image_bridge",
            output="screen",
            arguments=[
                '/world/default/model/robotmodel/link/camera_front/sensor/front_camera/image'
            ],
            parameters=[{"use_sim_time": use_sim_time}],
            remappings=[
                (
                    "/world/default/model/robotmodel/link/camera_front/sensor/front_camera/image",
                    "/camera/image_raw",
                ),
            ],
    )



    # Create the launch description and populate
    ld = LaunchDescription()

    # Declare the launch options
    ld.add_action(declare_namespace_cmd)
    ld.add_action(declare_use_sim_time_cmd)

    ld.add_action(declare_rviz_config_file_cmd)

    ld.add_action(declare_use_robot_state_pub_cmd)
    ld.add_action(declare_use_rviz_cmd)

    ld.add_action(gazebo_server)
    ld.add_action(gazebo_client)

    ld.add_action(ros_gz_bridge_cmd)
    ld.add_action(ros_gz_bridge2_cmd)
    ld.add_action(ros_gz_bridge3_cmd)
    ld.add_action(ros_gz_bridge4_cmd)
    ld.add_action(ros_gz_bridge5_cmd)
    ld.add_action(ros_gz_camera_bridge_cmd)

    # Add the actions to launch all of the navigation nodes
    ld.add_action(start_robot_state_publisher_cmd)

    ld.add_action(static_transform_cmd)
    ld.add_action(static_transform3_cmd)
    ld.add_action(static_transform4_cmd)

    ld.add_action(rviz_cmd)

    return ld
