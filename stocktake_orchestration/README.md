## Stocktake Orchestration node for Stocktake Simulation


#### Responsibilities
- Handles communication with web interface (including streaming maps, state information and tag scan results)
- Handles robot state and transitions of states
- Runs explore-lite (add package link) to fully map space
- Runs route and waypoint creation (NVIDIA-swagger)
- Runs period RFID scan and responsible for conducting full rounds of scanning

Requires
- Autonomy/Navigation backend - this is the other Stocktake package (add link)
- Web interface to be running as separate process (usually on different machine)
- (Optional) Gazebo simulator, if using simulation

