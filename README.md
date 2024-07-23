## Stocktake Robot Simulation
Recreation of capstone project in gazebo

\#NOTE: Omni Wheel STL files are from github.com/GUiRitter/OpenBase repo

NOTE: 27-4-24 mecanum wheels are working. Needs a bit of time at start to overcome the static friction but once done is able to move correctly

!! **IMPORTANT** !!
The omni wheels are not the correct type - they don't allow for planar motion. Need to either find a different SDF/STL model of the wheels or create yourself in FreeCAD.


### Structure
- **util/layout.py**: Generates store layout sdf model
- **include/robot.hh**: StocktakeRobot class with robot plugin for control and interface w ROS2
- **include/tag.hh**: Class representing a tag object
- **include/tagmanager.hh**: Manages generation of tag object through the world


### \#TODO
- Mecanum
	- Define the correct friction parameter values
	- Change wheel angle to 45 degrees (to maximise directional force)
	- \#NOTE: Just use diff drive for now and then try and implement mecanum later
- Simulation SDF
	- Procedurally generate store layout
	- Create different shelf SDF/STL models for the store
- Robot SDF
	- Wheel movement plugin
	- Setup ROS2 SLAM and Navigation for robot control
	- Add RFID sensor plugin
	- Add tag model and plugin
- Transforms
	- SLAM

### Features
- xx
- http://ode.org/wiki/index.php/Manual#Concepts
- https://classic.gazebosim.org/tutorials?tut=physics_params&cat=physics#Frictionparameters
- https://protobuf.dev/getting-started/cpptutorial/


