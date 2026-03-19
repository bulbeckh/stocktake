from launch import LaunchDescription
from launch.actions import ExecuteProcess
from ament_index_python.packages import get_package_share_directory

import os


def generate_launch_description():

    pkg_share = get_package_share_directory('stocktake_frontend')

    project_dir = os.path.join(pkg_share, '..', 'frontend')
    project_dir = os.path.abspath(project_dir)

    return LaunchDescription([
        ExecuteProcess(
            cmd=['npm', 'run', 'dev'],
            cwd=project_dir,
            output='screen',
            shell=False
        )
    ])
