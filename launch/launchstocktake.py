from launch.actions import ExecuteProcess

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, EmitEvent, LogInfo,
                            RegisterEventHandler)
from launch.conditions import IfCondition
from launch.events import matches_action
from launch.substitutions import (AndSubstitution, LaunchConfiguration,
                                  NotSubstitution)
from launch_ros.actions import LifecycleNode, Node
from launch_ros.event_handlers import OnStateTransition
from launch_ros.events.lifecycle import ChangeState
from lifecycle_msgs.msg import Transition

def generate_launch_description():

  gz_start = ExecuteProcess(cmd=['gz','sim','v1','-r', 'main.sdf'], output='screen')
  parameter_bridge = Node(
					package='ros_gz_bridge',
					executable='parameter_bridge',
					arguments=["/imu@sensor_msgs/msg/Imu[gz.msgs.IMU",
												"/lidar@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan",
												"/model/robotmodel/odometry@nav_msgs/msg/Odometry[gz.msgs.Odometry",
												"/model/robotmodel/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V",
												## NOTE: Only uncomment if we want to remove EKF fusion of IMU and just use solo wheel odometry
												#"/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V",
                        "/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"
  ])

  static_transforms = [
    Node(
						package='tf2_ros',
						executable='static_transform_publisher',
						arguments=['0', '0', '0.05', '0', '0', '0', 'base_link', 'robotmodel/robot-lidar/robot-lidar']
			),
		Node(
						package='tf2_ros',
						executable='static_transform_publisher',
						arguments=['0', '0', '0.1', '0', '0', '0', 'base_link', 'robotmodel/imu/imu-sensor']
			),
		Node(
						package='tf2_ros',
						executable='static_transform_publisher',
						arguments=['0', '0', '0', '0', '0', '0', 'base_link', 'robotmodel/robot-base']
		)
  ]
  
  localization = Node(
					package='robot_localization',
					executable='ekf_node',
					namespace='localization',
					name='ekf_filter_node',
					output='screen',
					parameters=['/home/henry/Documents/stocktake/config/ekf.yaml']
					#ros_arguments=['--log-level','INFO']
  )
  
  slam = LifecycleNode(
          package='slam_toolbox',
          executable='async_slam_toolbox_node',
          name='slam_toolbox', ## Needs to match with yaml config
          output='screen',
          parameters=[
              {'use_lifecycle_manager': False,
                'use_sim_time': True}],
          ros_arguments=[ #'--log-level', 'INFO',
            '--params-file', '/home/henry/Documents/stocktake/config/costmap.yaml'],
          namespace=''
  )

  rviz2 = Node(
            package='rviz2',
            executable='rviz2',
            output='screen',
            arguments=['-d', '/home/henry/Documents/stocktake/config/newconf.rviz']
  )

  global_costmap =  LifecycleNode(
				package='nav2_costmap_2d',
				executable='nav2_costmap_2d',
				name='global_costmap',
				namespace='global_costmap',
				output='screen',
				parameters=['/home/henry/Documents/stocktake/config/costmap.yaml'],
				ros_arguments=['--log-level','WARN']
  )
	
  local_costmap = LifecycleNode(
				package='nav2_costmap_2d',
				executable='nav2_costmap_2d',
				name='local_costmap',
				namespace='',
				output='screen',
				parameters=['/home/henry/Documents/stocktake/config/costmap.yaml'],
				ros_arguments=['--log-level','WARN']
  )

  planner_server = LifecycleNode(
				package='nav2_planner',
				executable='planner_server',
				name='planner_server',
				namespace='planner_server',
				output='screen',
				parameters=['home/henry/Documents/stocktake/config/controller.yaml'],
				ros_arguments=['--log-level', 'WARN']
  )

  ## Configure/Activate
  slam_configure = EmitEvent(
    event=ChangeState(
      lifecycle_node_matcher=matches_action(slam),
      transition_id=Transition.TRANSITION_CONFIGURE
    )
  )
  slam_activate = EmitEvent(
		event=ChangeState(
			lifecycle_node_matcher=matches_action(slam),
			transition_id=Transition.TRANSITION_ACTIVATE
		)
  )

  gcostmap_configure = EmitEvent(
		event=ChangeState(
			lifecycle_node_matcher=matches_action(global_costmap),
			transition_id=Transition.TRANSITION_CONFIGURE
		)
  )

  gcostmap_activate = EmitEvent(
		event=ChangeState(
			lifecycle_node_matcher=matches_action(global_costmap),
			transition_id=Transition.TRANSITION_ACTIVATE
		)
  )

  lcostmap_configure = EmitEvent(
		event=ChangeState(
			lifecycle_node_matcher=matches_action(local_costmap),
			transition_id=Transition.TRANSITION_CONFIGURE
		)
  )

  lcostmap_activate = EmitEvent(
		event=ChangeState(
			lifecycle_node_matcher=matches_action(local_costmap),
			transition_id=Transition.TRANSITION_ACTIVATE
		)
  )

  planner_server_configure = EmitEvent(
		event=ChangeState(
			lifecycle_node_matcher=matches_action(planner_server),
			transition_id=Transition.TRANSITION_CONFIGURE
		)
  )

  planner_server_activate = EmitEvent(
		event=ChangeState(
			lifecycle_node_matcher=matches_action(planner_server),
			transition_id=Transition.TRANSITION_ACTIVATE
		)
  )

  ## NOTE: Use GroupAction here
  ## Transitions (NOTE: Can we do configuration/activation of costmap nodes in parallel)
  slam_transitions = RegisterEventHandler(
    OnStateTransition(
      target_lifecycle_node=slam,
      goal_state="inactive",
      entities=[
        LogInfo(msg="[CALLBACK] slam reached inactive, now activating"),
				slam_activate
       ]
    )
  )

  slam_activate_transition = RegisterEventHandler(
		OnStateTransition(
			target_lifecycle_node=slam,
			goal_state="active",
			entities=[
				LogInfo(msg="[CALLBACK] slam reached active, now configuring global costmap and local costmap"),
				gcostmap_configure,
				lcostmap_configure
			]
		)
  )

  gca = RegisterEventHandler(
    OnStateTransition(
      target_lifecycle_node=global_costmap,
      goal_state="inactive",
      entities=[
        LogInfo(msg="[CALLBACK] GC reached inactive, now activating"),
				gcostmap_activate
       ]
    )
  )

  lca = RegisterEventHandler(
    OnStateTransition(
      target_lifecycle_node=local_costmap,
      goal_state="inactive",
      entities=[
        LogInfo(msg="[CALLBACK] LC reached inactive, now activating"),
				lcostmap_activate
       ]
    )
  )

  lc_activated = RegisterEventHandler(
    OnStateTransition(
      target_lifecycle_node=local_costmap,
      goal_state="active",
      entities=[
        LogInfo(msg="[CALLBACK] local_costmap activated"),
				planner_server_configure
       ]
    )
  )

  planner_server_inact = RegisterEventHandler(
    OnStateTransition(
      target_lifecycle_node=planner_server,
      goal_state="inactive",
      entities=[
        LogInfo(msg="[CALLBACK] planner server inactive"),
				planner_server_activate
       ]
    )
  )
	
	

  transitions = [slam_transitions, slam_activate_transition, gca, lca, lc_activated, planner_server_inact]

  return LaunchDescription([
		gz_start,
		parameter_bridge,
		*static_transforms,
    localization,
    slam,
    rviz2,
		global_costmap,
		local_costmap,
    planner_server,
		*transitions,
		slam_configure ## Kicks-off SLAM -> costmap transitions
  ])

