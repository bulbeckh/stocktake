from launch import LaunchDescription
from launch.actions import ExecuteProcess
from ament_index_python.packages import get_package_share_directory, get_package_prefix

import os


def generate_launch_description():

    #pkg_share = get_package_share_directory('stocktake_frontend')
    #project_dir = os.path.join(pkg_share, 'frontend')
    #project_dir = os.path.abspath(project_dir)


    ## TODO While this path is not hardcoded, it's not particularly portable - it's
    ##  a workaround to prevent next.js from running under the install/ directory which
    ##  has caused permission issues previously - need to fix this workaround.
    project_dir = os.path.join(
            get_package_prefix('stocktake_frontend'),
            '../../src/stocktake/stocktake_frontend/frontend')

    return LaunchDescription([
        ExecuteProcess(
            cmd=['npm', 'run', 'dev'],
            cwd=project_dir,
            output='screen',
            shell=False
        )
    ])
