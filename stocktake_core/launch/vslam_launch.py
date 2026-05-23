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
                'publish_tf': False
            }],
    )

    # Create the launch description and populate
    ld = LaunchDescription()

    ld.add_action(stella_vslam_cmd)

    return ld
