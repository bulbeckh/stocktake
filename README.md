## Stocktake Robot Simulation
Recreation of capstone project in gazebo

\#NOTE: Omni Wheel STL files are from github.com/GUiRitter/OpenBase repo

!! **IMPORTANT** !!
The omni wheels are not the correct type - they don't allow for planar motion. Need to either find a different SDF/STL model of the wheels or create yourself in FreeCAD.

### \#TODO
- Mecanum
	- Define the correct friction parameter values
	- Change wheel angle to 45 degrees (to maximise directional force)
- Simulation
	- Procedurally generate store layout
	- Create different shelf SDF/STL models for the store
- Robot
	- Setup ROS2 SLAM and Navigation for robot control
	- Add RFID sensor plugin
	- Add tag model and plugin

### Features
- xx
- http://ode.org/wiki/index.php/Manual#Concepts
- https://classic.gazebosim.org/tutorials?tut=physics_params&cat=physics#Frictionparameters
- https://protobuf.dev/getting-started/cpptutorial/


