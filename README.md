
## Stocktake Robot Simulation
Recreation of capstone project in gazebo

### Running
```bash
source /opt/ros/jazzy/setup.bash
source setup.sh
ros2 launch launch3.py
```

Added ros2 explore-lite port (https://github.com/robo-friends/m-explore-ros2/tree/main)
```bash
source install/setup.bash
ros2 launch explore_lite explore.launch.py
```

Added stella_vslam (will replace 2D lidar soon) (https://github.com/stella-cv/stella_vslam_ros)
```bash
source install/setup.bash
## In ros2_ws dir (will move to main package soon)
ros2 run stella_vslam_ros run_slam -v orb_vocab.fbow -c gz_camera.yaml --ros-args -p publish_tf:=false
```


## Navigation Stack
We use the [Navigation2](https://github.com/ros-navigation/navigation2) ROS2 package for the planning, control, state estimation, and behaviour tree.

### Controller
- `nav2_mppi_controller::MPPIController`
    - ConstraintCritic
    - CostCritic
    - GoalCritic
    - GoalAngleCritic
    - PathAlignCritic
    - PathFollowCritic
    - PathAngleCritic
    - PreferForwardCritic

### Planner
- `nav2_navfn_planner::NavfnPlanner`

### Local Costmap
- `nav2_costmap_2d::VoxelLayer`
- `nav2_costmap_2d::InflationLayer`

### Global Costmap
- `nav2_costmap_2d::StaticLayer`
- `nav2_costmap_2d::InflationLayer`
- `nav2_costmap_2d::ObstacleLayer`

### Behaviour Tree
- `nav2_bt_navigator::NavigateToPoseNavigator`
- `nav2_bt_navigator::NavigateThroughPosesNavigator`

### Smoother
- `nav2_smoother::SimpleSmoother`

TODO Remove route and docking servers

## Behaviour Tree Overview
`TODO`

## Map Exploration
[m-explore-ros2](https://github.com/robo-friends/m-explore-ros2/tree/main) ROS2 Explore Lite port

### Libraries
- [OpenBase](https://github.com/GUiRitter/OpenBase) Omni Wheel STL files
- [Navigation2](https://github.com/ros-navigation/navigation2) Navigation planners, costmaps


