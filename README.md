

## Stocktake Robot Simulation
Recreation of capstone project in gazebo

### Libraries
- Omni Wheel STL files are from [OpenBase](https://github.com/GUiRitter/OpenBase) repo
- Navigation planners, costmap, and control uses [Navigation2](https://github.com/ros-navigation/navigation2)

NOTE: 27-4-24 mecanum wheels are working. Needs a bit of time at start to overcome the static friction but once done is able to move correctly
NOTE: The omni wheels are not the correct type - they don't allow for planar motion. Need to either find a different SDF/STL model of the wheels or create yourself in FreeCAD.

### Structure
- **util/layout.py**: Generates store layout sdf model
- **util/mecanum_angle.py**: Generates the mecanum SDF models
- **include/robot.hh**: StocktakeRobot class with robot plugin for control and interface w ROS2
- **include/rfidscanner.hh**: [WIP] Implementation of Gazebo RFID scanning sensor
- **include/rfidtagmanager.hh**: Manages generation of tag object through the world
- **launch/**: ROS2 Launch files
- **config/**: Navigation2 configuration files for controller, amcl, ekf, and costmaps
- **models/**: Location of SDF models for Mecanum wheels, robot base, and store layout

### `\#TODO`
- [ ] Mecanum and Omni-directional Movement
- [ ] Procedural Store Generation
- [x] Robot SDF
- [ ] RFID Tag, Tag Manager, and Sensor (Feature Contribution)
- [x] SLAM and EKF
- [x] Costmaps
- [ ] Planner and Controller
- [ ] ROS Package Setup
- [ ] Behaviour Tree or Robot State Machine + Simulation of stocktake process

### Features
- http://ode.org/wiki/index.php/Manual#Concepts
- https://classic.gazebosim.org/tutorials?tut=physics_params&cat=physics#Frictionparameters

### Issue Log
10th August
- Costmap is now publishing. Next is the controller and planner plugins.
9th August
- Big fix with the EKF filter. The imu and wheel odom are now fused and working well. map and odom frames stay consistently close and /map is updated using the slam\_toolbox. The fix was to remove some of the imu0\_config and odom0\_config values as they were being needlessly fed into the state estimator.

#### 8th August
- Removed the EKF filter - covariance was way too high and causing issues. Instead changed just to wheeel odometry.
- UPDATE: Fixed the wheel separation issue. EKF fusion works better but still not great when just moving laterally/translationally
- Can now either skip the EKF fusion by:
	1. Changing the diff drive output topic in the robot.sdf XML file to /tf
	2. Bridging /tf between ros2 and gz
	3. Removing the localization node from launch file

#### 6th August
- transforms are working and map is updating. Errors when map slowly moves away from main area. Also relies **a lot** on rotational movement and not just translational movement.

#### 5th August
**Potential Issues**
- imu orientation
- the diff drive odometry system may be publishing in wrong frame

