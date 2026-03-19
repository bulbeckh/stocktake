
## Stocktake Robot Simulation
Recreation of capstone project in gazebo

### Building

#### Install build dependencies
```bash
sudo apt install python3-virtualenv
```

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws
git clone --recurse-submodules https://github.com/bulbeckh/stocktake.git
source /opt/ros/jazzy/setup.bash
```

Create virtual environment at root of our workspace folder
```bash
virtualenv ./venv
source ./venv/bin/activate
pip3 install colcon
touch ./venv/COLCON_IGNORE
```

We then need to install the nvidia-swagger python package. You can find full install install instructions [in the repo](https://github.com/nvidia-isaac/SWAGGER).
```bash
cd ~
git clone git@github.com:nvidia-isaac/SWAGGER.git
cd SWAGGER
git lfs pull

## Install nvidia-swagger deps
sudo apt update && sudo apt install -y libgl1-mesa-glx libglib2.0-0

## Install package in our virtual environment
pip install -e .
```

`TODO` Add stella_vslam instructions

Build all packages
```bash
cd ~/ros_ws
colcon build --symlink-install
```

### Launch
TODO
```bash
ros2 launch launch3.py
```

Added stella_vslam (will replace 2D lidar soon) (https://github.com/stella-cv/stella_vslam_ros)
```bash
source install/setup.bash
## In ros2_ws dir (will move to main package soon)
ros2 run stella_vslam_ros run_slam -v orb_vocab.fbow -c gz_camera.yaml --ros-args -p publish_tf:=false
```

### Packages
| Package | Description |
| --- | --- |
| `stocktake_core` | Nodes for mapping and autonomous navigation (Nav2) and simulation (Gazebo) |
| `stocktake_orchestration` | Nodes for robot state machine and interface with frontend |
| `stocktake_nvidia_swagger` | Nodes for waypoint generation from map, via nvidia-swagger package |
| `stocktake_nvidia_swagger_msgs` | Custom messages for waypoint generation |
| `stocktake_frontend` | Frontend for stocktake/robot control web interface
| `m-explore-ros2` | ROS2 package for map exploration |


## Navigation Stack
`TODO` We use the [Navigation2](https://github.com/ros-navigation/navigation2) ROS2 package for the planning, control, state estimation, and behaviour tree.

## Behaviour Tree Overview
`TODO`

## Map Exploration
[m-explore-ros2](https://github.com/robo-friends/m-explore-ros2/tree/main) ROS2 Explore Lite port

### Libraries
- [OpenBase](https://github.com/GUiRitter/OpenBase) Omni Wheel STL files
- [Navigation2](https://github.com/ros-navigation/navigation2) Navigation planners, costmaps

