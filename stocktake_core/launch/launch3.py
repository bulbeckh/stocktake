import os
import yaml
import tempfile

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription,
                            OpaqueFunction, RegisterEventHandler, GroupAction)
from launch.conditions import IfCondition, UnlessCondition 
from launch.event_handlers import OnShutdown
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node, SetParameter, PushROSNamespace
from nav2_common.launch import RewrittenYaml

"""
This launch file creates the core elements of the stocktake system, including:

- Gazebo simulation (including optional client)
- Static transforms 


"""
def deep_merge(a: dict, b: dict) -> dict:
    """
    Recursively merge dict b into dict a.
    Values in b override values in a.
    """
    result = dict(a)

    for key, value in b.items():
        if (
            key in result
            and isinstance(result[key], dict)
            and isinstance(value, dict)
        ):
            result[key] = deep_merge(result[key], value)
        else:
            result[key] = value

    return result


def load_yaml(path: str) -> dict:
    with open(path, "r") as f:
        return yaml.safe_load(f) or {}


def merge_param_files(file_a: str, file_b: str) -> str:
    """
    Merge two ROS 2 parameter YAML files and return
    the path to a temporary merged YAML file.
    """
    params_a = load_yaml(file_a)
    params_b = load_yaml(file_b)

    merged = deep_merge(params_a, params_b)

    tmp = tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".yaml",
        delete=False,
    )

    yaml.safe_dump(merged, tmp)
    tmp.close()

    return tmp.name


## TODO Early exit via RuntimeError leaves un-killed nodes. Need to fix
def validate_robot_type(context):
    rtype = LaunchConfiguration('robot_type').perform(context)
    if rtype not in ['lidar', 'rgbd', 'stereo', 'mono']:
        raise RuntimeError(
            f"Invalid robot type '{rtype}'. "
            f"Must be one of: lidar, rgbd, stereo, mono"
        )

    return []

def navigation_opaque_function(context, stocktake_core_dir, bringup_dir, *args, **kwargs):
    '''We need to wrap this in an opaque function so that we have access to the 'robot_type'
    launch configuration. Otherwise, we are unable to use the yaml merge functionality'''

    robot_type = LaunchConfiguration('robot_type').perform(context)

    ## Merge the nav2 main and costmaps file and then overwrite some values
    if robot_type=='lidar':
        costmap_path = os.path.join(stocktake_core_dir, 'config', 'lidar_costmaps.yaml')
    elif robot_type=='rgbd':
        costmap_path = os.path.join(stocktake_core_dir, 'config', 'rgbd_costmaps.yaml')
    elif robot_type=='stereo':
        ## TODO Add new stereo costmap configuration (might be same as rgbd)
        costmap_path = os.path.join(stocktake_core_dir, 'config', 'rgbd_costmaps.yaml')
    elif robot_type=='mono':
        ## TODO Add new mono costmap configuration (might be same as rgbd)
        costmap_path = os.path.join(stocktake_core_dir, 'config', 'rgbd_costmaps.yaml')

    merged_params_file = merge_param_files(
            os.path.join(stocktake_core_dir, 'config', 'nav2_params.yaml'),
            costmap_path
    )

    ## Launch Rviz2 with Nav2 Rviz2 configuration
    rviz_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(bringup_dir, 'launch', 'rviz_launch.py')),
        launch_arguments={
            'namespace': LaunchConfiguration('namespace'),
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'rviz_config': os.path.join(stocktake_core_dir, 'config', 'nav2_default_view.rviz'),
        }.items(),
    )

    ## Remap the tf topics to relative namespaces so we can add namespace prefixes
    tf_remappings = [('/tf', 'tf'), ('/tf_static', 'tf_static')]

    navigation_lidar_slam_cmd = GroupAction(
        [
            PushROSNamespace(condition=IfCondition(LaunchConfiguration('use_namespace')), namespace=LaunchConfiguration('namespace')),
            Node(
                condition=IfCondition(LaunchConfiguration('use_composition')),
                name='nav2_container',
                package='rclcpp_components',
                executable='component_container_isolated',
                parameters=[merged_params_file, {'autostart': 'True'}],
                arguments=['--ros-args', '--log-level', 'info'],
                remappings=tf_remappings,
                output='screen',
            ),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(bringup_dir, 'launch', 'slam_launch.py')
                ),
                ## With the 2D lidar sensor, we use slam_toolbox (launched inside container)
                condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'lidar' "])),
                launch_arguments={
                    'namespace': LaunchConfiguration('namespace'),
                    'use_sim_time': LaunchConfiguration('use_sim_time'),
                    'autostart': 'True',
                    'use_respawn': LaunchConfiguration('use_respawn'),
                    'params_file': LaunchConfiguration('slam_params_file'),
                    #'params_file': slam_params_configured,
                }.items(),
            ),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    # NOTE We use our own modified navigation_launch.py in which we remove the collision_monitor.
                    # This is only for the non-lidar sensors and is due to a current issue where we get ~0.8Hz rate
                    # for the pointcloud gz to ros bridge. Once fixed, we can bring back in. Also note, since we
                    # removed the collision monitor, we bridge cmd_vel on /cmd_vel_smoothed (output of velocity_smoother)
                    # rather than /cmd_vel (output of collision monitor)
                    #
                    # When we are not using the rgbd, we can launch the nav2_bringup navigation_launch
                    #os.path.join(bringup_dir, 'launch', 'navigation_launch.py')
                    os.path.join(stocktake_core_dir, 'launch', 'navigation_launch.py')
                ),
                condition=UnlessCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'lidar' "])),
                launch_arguments={
                    'namespace': LaunchConfiguration('namespace'),
                    'use_sim_time': LaunchConfiguration('use_sim_time'),
                    'autostart': 'True',
                    'params_file': merged_params_file,
                    'use_composition': LaunchConfiguration('use_composition'),
                    'use_respawn': LaunchConfiguration('use_respawn'),
                    'container_name': 'nav2_container',
                }.items(),
            ),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(bringup_dir, 'launch', 'navigation_launch.py')
                ),
                condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'lidar' "])),
                launch_arguments={
                    'namespace': LaunchConfiguration('namespace'),
                    'use_sim_time': LaunchConfiguration('use_sim_time'),
                    'autostart': 'True',
                    'params_file': merged_params_file,
                    'use_composition': LaunchConfiguration('use_composition'),
                    'use_respawn': LaunchConfiguration('use_respawn'),
                    'container_name': 'nav2_container',
                }.items(),
            ),
        ]
    )

    ## Start map server if we are not using slam_toolbox (originally launched alongside slam_toolbox in slam_launch.py)
    start_map_server = GroupAction(
        condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' != 'lidar' "])),
        actions=[
            SetParameter('use_sim_time', 'True'),
            Node(
                package='nav2_map_server',
                executable='map_saver_server',
                output='screen',
                respawn=LaunchConfiguration('use_respawn'),
                respawn_delay=2.0,
                arguments=['--ros-args', '--log-level', 'info'],
                parameters=[merged_params_file],
            ),
            Node(
                package='nav2_lifecycle_manager',
                executable='lifecycle_manager',
                name='lifecycle_manager_slam',
                output='screen',
                arguments=['--ros-args', '--log-level', 'info'],
                parameters=[{'autostart': True}, {'node_names': ['map_saver']}],
            ),
        ]
    )

    return [start_map_server, navigation_lidar_slam_cmd, rviz_cmd]

def generate_launch_description() -> LaunchDescription:
    # Get package directories
    bringup_dir        = get_package_share_directory('nav2_bringup')
    stocktake_core_dir = get_package_share_directory('stocktake_core')

    # Configures robot_type [lidar, rgbd, stereo, mono]
    robot_type = LaunchConfiguration('robot_type')

    # Create the launch configuration variables
    namespace     = LaunchConfiguration('namespace')
    use_namespace = LaunchConfiguration('use_namespace')
    use_sim_time  = LaunchConfiguration('use_sim_time')

    ## TODO These are now not used as the params yaml merger gets paths directly
    params_file      = LaunchConfiguration('params_file')
    costmaps_file    = LaunchConfiguration('costmaps_file')
    slam_params_file = LaunchConfiguration('slam_params_file')

    # Nav2 configurations
    use_composition         = LaunchConfiguration('use_composition')
    use_intra_process_comms = LaunchConfiguration('use_intra_process_comms')
    use_respawn             = LaunchConfiguration('use_respawn')


    # Declare the launch arguments
    declare_namespace_cmd = DeclareLaunchArgument(
        'namespace', default_value='', description='Top-level namespace'
    )

    declare_use_namespace_cmd = DeclareLaunchArgument(
        'use_namespace', default_value='False', description='Whether we use namespace'
    )

    declare_use_sim_time_cmd = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation (Gazebo) clock if true',
    )

    declare_params_file_cmd = DeclareLaunchArgument(
        'params_file',
        default_value=os.path.join(stocktake_core_dir, 'config', 'nav2_params.yaml'),
        description='Full path to the ROS2 parameters file to use for all launched nodes',
    )

    declare_costmaps_file_cmd = DeclareLaunchArgument(
        'costmaps_file',
        default_value=os.path.join(stocktake_core_dir, 'config', 'costmaps.yaml'),
        description='Stocktake costmaps yaml file',
    )

    declare_slam_params_file_cmd = DeclareLaunchArgument(
        'slam_params_file',
        default_value=os.path.join(stocktake_core_dir, 'config', 'slam_toolbox.yaml'),
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

    declare_robot_type_cmd = DeclareLaunchArgument(
        'robot_type',
        default_value='lidar',
        description='robot_type: lidar, rgbd, stereo, mono'
    )

    ## Validate that we have chosen one of the four robot types
    robot_type_validation_cmd = OpaqueFunction(function=validate_robot_type)
    
    ## Re-configure certain configuration files
    """
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
    """


    ## Transform tree
    # NOTE We could use robot_state_publisher for this but less control over transforms
    #
    # in lidar mode we have the following tf's:
    #
    # map -> odom (slam_toolbox)
    # odom -> base_link (wheel odom bridge from gz)
    # base_link -> lidar (static)
    # 
    # in the vslam modes (rgbd, stereo, mono) we have the following tf's:
    # map -> <camera_frames> (stella-vslam) (NOTE How exactly are two stereo camera frames transformed to optical frame)
    # odom -> base_link (wheel odom) (NOTE is this redundant for vslam)
    # base_link -> <camera_frames> (static)
    # <camera_frames> -> camera_optical (static, this is required 
    #       to correct the frame convention between opencv and ros2)

    base_lidar_static_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'lidar' "])),
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

    base_link_robot_base_cmd = Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='base_link_robot_base_transform',
            namespace='',
            output='screen',
            arguments=[
                '--frame-id',
                'base_link',
                '--child-frame-id',
                'robot_base',
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
            ],
    )
    
    ## NOTE May be able to override the camera frame id

    base_front_camera_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' in ['rgbd','mono'] "])),
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_publisher5',
            namespace='',
            output='screen',
            arguments=[
                '--frame-id',
                'robot_base',
                '--child-frame-id',
                'store_layout/robotmodel/camera_front/camera'
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
            ],
    )

    front_camera_optical_cmd = Node(
            condition=IfCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' in ['rgbd','mono'] "])),
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_publisher6',
            namespace='',
            output='screen',
            arguments=[
                '0', '0', '0', # x y z
                '-1.57079632679', '0', '-1.57079632679',  # roll pitch yaw
                'store_layout/robotmodel/camera_front/camera',
                'optical_camera_frame'
            ],
            parameters=[
                {'use_sim_time': use_sim_time}
            ],
    )

    simulation_launch_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(stocktake_core_dir, 'launch', 'simulation.py')),
        launch_arguments={
            'robot_type': robot_type,
        }.items(),
    )

    ## Octomap nodes (only run when not using 'lidar' robot_type)
    # NOTE occupancy_{min,max}_z is used to generate the 2D occupancy grid projection - need to decide on range to use
    octomap_node_cmd = Node(
            condition=UnlessCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'lidar' "])),
            package='octomap_server',
            executable='octomap_server_node',
            name='octomap_server',
            #output='screen',
            remappings=[
                ('cloud_in', '/camera/pointcloud'),
                ('projected_map', '/map'),
                ('projected_map_updates', '/map_updates'),
            ],
            parameters=[{
                'occupancy_max_z': 1.0,
                'occupancy_min_z': -1.0,
            }],
    )

    # TODO Either fix the LD_PRELOAD workaround (via patch) or retrieve lib directory differentely
    octomap_rviz_cmd = Node(
            condition=UnlessCondition(PythonExpression(["'", LaunchConfiguration('robot_type'), "' == 'lidar' "])),
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            output='screen',
            arguments=['-d', os.path.join(stocktake_core_dir, 'config', 'octomap.rviz')],
            additional_env={
                'LD_PRELOAD': '/usr/lib/x86_64-linux-gnu/liboctomap.so'
            }
    )

    # Create the launch description and populate
    ld = LaunchDescription()

    # Declare the launch options
    ld.add_action(declare_namespace_cmd)
    ld.add_action(declare_use_namespace_cmd)
    ld.add_action(declare_use_sim_time_cmd)
    ld.add_action(declare_params_file_cmd)
    ld.add_action(declare_costmaps_file_cmd)
    ld.add_action(declare_slam_params_file_cmd)
    ld.add_action(declare_use_composition_cmd)
    ld.add_action(declare_use_intra_process_comms_cmd)

    ld.add_action(declare_use_respawn_cmd)

    ld.add_action(simulation_launch_cmd)
    
    ## static transforms
    ld.add_action(base_lidar_static_cmd)
    ld.add_action(base_link_robot_base_cmd)
    ld.add_action(base_front_camera_cmd) # Used on mono and rgbd camera setups
    ld.add_action(front_camera_optical_cmd)



    ld.add_action(octomap_node_cmd)
    ld.add_action(octomap_rviz_cmd)

    #ld.add_action(start_map_server)
    #ld.add_action(navigation_lidar_slam_cmd)
    #ld.add_action(rviz_cmd)
    ld.add_action(OpaqueFunction(function=navigation_opaque_function,
                                 args=[stocktake_core_dir, bringup_dir]))

    ld.add_action(declare_robot_type_cmd)
    ld.add_action(robot_type_validation_cmd)


    return ld
