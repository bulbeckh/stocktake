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

    declare_robot_type_cmd = DeclareLaunchArgument(
        'robot_type',
        default_value='lidar',
        description='robot_type: lidar, rgbd, stereo, mono'
    )

    ## Octomap nodes
    #TODO Don't use absolute paths for the config file. Issue is that config/ directory is not in any package
    # but rather in the wider stocktake repo root directory. Probably need to move each to a package
    stella_vslam_cmd = Node(
            package='stella_vslam_ros',
            executable='run_slam',
            name='stella_vslam_node',
            output='screen',
            arguments=[
                '-v',
                '/tmp/orb_vocab.fbow',
                '-c',
                ## TODO Make this configurable based on the robot_type (rgbd, stereo, and mono should all have different yamls)
                '/workspaces/stocktake-alt/src/stocktake/stocktake_core/config/gz_camera_rgbd.yaml'
            ],
            parameters=[{
                'publish_tf': True,
                #'camera_frame': 'store_layout/robotmodel/camera_front/camera',
                'camera_frame': 'optical_camera_frame',
                'use_sim_time': True,
                'transform_tolerance': 1.0
            }],
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
                'base_frame_id': 'robot_base',
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

    ld.add_action(stella_vslam_cmd)
    ld.add_action(octomap_node_cmd)
    ld.add_action(octomap_rviz_cmd)

    return ld
