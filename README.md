
## Stocktake Robot Simulation
Recreation of capstone project in gazebo

### Running
```bash
source setup.sh
ros2 launch launch3.py
```

### Libraries
- [OpenBase](https://github.com/GUiRitter/OpenBase) Omni Wheel STL files
- [Navigation2](https://github.com/ros-navigation/navigation2) Navigation planners, costmaps

### Structure
- **util/layout.py**: Generates store layout sdf model
- **util/mecanum_angle.py**: Generates the mecanum SDF models
- **include/robot.hh**: StocktakeRobot class with robot plugin for control and interface w ROS2
- **include/rfidscanner.hh**: [WIP] Implementation of Gazebo RFID scanning sensor
- **include/rfidtagmanager.hh**: Manages generation of tag object through the world
- **launch/**: ROS2 Launch files
- **config/**: Navigation2 configuration files for controller, amcl, ekf, and costmaps
- **models/**: Location of SDF models for Mecanum wheels, robot base, and store layout


