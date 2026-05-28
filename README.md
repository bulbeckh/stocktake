<div align="center">

# Stocktake

Autonomous stocktaking robot for retail and industrial environments

</div>

<img src="./docs/res/output.gif" alt="simgif" width="100%" />

### Building
Currently, this repository has only been tested on Ubuntu 24.04, with ros2-jazzy and gz-harmonic. At the very least we need ROS2 Jazzy (specifically `ros2-jazzy-desktop` [instructions here](https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html)) and Gazeob simulator (harmonic).

TODO Not enoug detail here - add

We need a few dependencies first, specifically `git-lfs`
```bash
sudo apt install git git-lfs
```

Clone the repository and submodules
```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
git clone --recurse-submodules https://github.com/bulbeckh/stocktake.git
```

Pull our nvidia-swagger lfs
```bash
cd SWAGGER
git lfs pull
```

#### Docker setup
```bash
## Build image
sudo docker build -f docker/Dockerfile -t stocktake .

## Start container
sudo docker compose up dev
sudo docker compose exec dev bash
```

Build all packages NOTE rename the stockake folder from stocktake-alt (only that way because .venv was accidentally placed in /workspaces/stocktake and the mount overwrote it)
```bash
cd /workspaces/stocktake-alt
colcon build
source ./install/setup.bash
```

#### Install other dependencies
NOTE These instructions are not required for docker now. Need to update them for users not planning to use docker

In order to run our nvidia-swagger ROS2 node, we have a python dependency on the nvidia-swagger python package.

Create virtual environment at root of our workspace folder
```bash
python3 -m venv .venv
source ./venv/bin/activate
touch ./venv/COLCON_IGNORE
```

Install the nvidia-swagger python package (while in the virtual env). You can find full install install instructions [in the repo](https://github.com/nvidia-isaac/SWAGGER).
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

### Launch
We have four different robot types to choose from, each differing based on the camera array. Currently, `rgbd` is the most stable.

| Robot Type | Description |
| --- | --- |
| `lidar` | Lidar scanner, slam_toolbox, nav2 |
| `rgbd` | Depth camera, stella-vslam, nav2 |
| `stereo` | Dual color cameras, stella-vslam, nav2 |
| `mono` | Single color camera, stella-vslam, nav2 |

If running in docker, need to run on the host:
```bash
xhost +local:docker
```

We can get a bash terminal in our container with
```bash
sudo docker compose exec dev bash
```

We should first build our packages. It is important that we use --symlink-install. 
```bash
# Inside container
source /opt/ros/jazzy/setup.bash
cd /workspaces/stocktake-alt
colcon build --symlink-install
source install/setup.bash
```

If we are using anything other robot type than `lidar`, we need to also build stella-vslam-ros. TODO just move vslam into the stocktake-alt workspace - all dependencies are satisfied through the dockerfile anyway and it is a very quick colcon build.
```bash
# Inside container
cd /workspaces/stocktake-vslam
colcon build --symlink-install
source install/setup.bash
```

#### Launching navigation
Our `stocktake_core` packages handles all the simulation bringup and navigation stack. We supply the robot type here as a launch arg.
```bash
ros2 launch stocktake_core launch3.py robot_type:=<lidar, rgbd, stereo, mono>
```

#### Launching orchestration
TODO

#### Launching frontend
TODO

NOTE Each of these will soon be launched together via a single launch file
NOTE Using stocktake_orchestration2 package - will soon remove original and replace with 'stocktake_orchestration2'
```bash
# Run orchestration
ros2 run stocktake_orchestration2 stocktake_orchestration

# Launch core + navigation
ros2 launch stocktake_core launch3.py

# Run the explore-lite node (orchestration attempts to make it start in paused state) (NOTE Default launch file for now but will soon use an updated launch command)
ros2 launch explore_lite explore.launch.py
```

We also need to run the swagger server node but this requires us to be using our virtual environment
```bash
cd ~/ros2_ws
source ./venv/bin/activate

# Run the SWAGGER server node
ros2 run stocktake_nvidia_swagger server_node
```

We then start the web interface, navigate to the web page, which should automatically connect to the orchestration node websocket.
```bash
# Start the web interface
cd stocktake/stocktake_frontend/frontend
npm run dev
```

TODO
Added stella_vslam (will replace 2D lidar soon) (https://github.com/stella-cv/stella_vslam_ros)
```bash
source install/setup.bash
## In ros2_ws dir (will move to main package soon)
ros2 run stella_vslam_ros run_slam -v orb_vocab.fbow -c gz_camera_rgbd.yaml --ros-args -p publish_tf:=false
```

### Packages
| Package | Description |
| --- | --- |
| `stocktake_core` | Nodes for mapping and autonomous navigation (Nav2) and simulation (Gazebo) |
| `stocktake_orchestration` | Nodes for robot state machine and interface with frontend |
| `stocktake_nvidia_swagger` | Nodes for waypoint generation from map, via nvidia-swagger package |
| `stocktake_nvidia_swagger_msgs` | Custom messages for waypoint generation |
| `stocktake_frontend` | Frontend for stocktake/robot control web interface
| `explore-ros2-action` | Fork of [m-explore-ros2](https://github.com/robo-friends/m-explore-ros2) frontier-based exploration algorithm, with action server instead of node auto-start |
| `gazebo-rfid-plugin` | RFID scanning system plugin for Gazebo sim, modelling realistic RFID scan effects |

## Autonomy Stack
NOTE: Still experimenting with mapping/navigation stacks, including visual SLAM implementations.

`TODO` We use the [Navigation2](https://github.com/ros-navigation/navigation2) ROS2 package for the planning, control, state estimation, and behaviour tree.

Map exploration is currently done using a frontier-based exploration algorithm from `m-explore-ros`.

Stella-VSLAM provides us with stereo (and mono) camera-based SLAM from which we can extract a world representation.

### Libraries
- [OpenBase](https://github.com/GUiRitter/OpenBase) Omni Wheel STL files
- [Navigation2](https://github.com/ros-navigation/navigation2) Navigation planners, costmaps

