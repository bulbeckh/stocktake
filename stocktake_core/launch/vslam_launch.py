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

    ## Republish the /tf and /tf_static topic to vslam_tf

    '''
    relay1_cmd = Node(
            package='topic_tools',
            executable='relay',
            name='relay1',
            output='screen',
            arguments=[
                '/tf',
                '/vslam_tf'
            ],
    )

    relay2_cmd = Node(
            package='topic_tools',
            executable='relay',
            name='relay2',
            output='screen',
            arguments=[
                '/tf_static',
                '/vslam_tf_static'
            ],
    )
    '''

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
                '/workspaces/stocktake-alt/src/stocktake/config/gz_camera_rgbd.yaml'
            ],
            parameters=[{
                'publish_tf': True,
                #'camera_frame': 'store_layout/robotmodel/camera_front/left_camera',
                'camera_frame': 'optical_camera_frame',
                'use_sim_time': True,
                'transform_tolerance': 1.0
            }],
            #remappings=[
                #('/tf', '/vslam_tf'),
                #('/tf_static', '/vslam_tf_static')
            #],
    )

    # Create the launch description and populate
    ld = LaunchDescription()

    ld.add_action(stella_vslam_cmd)
    #ld.add_action(relay1_cmd)
    #ld.add_action(relay2_cmd)

    return ld
