## TOPICS 
**imu**
/imu : gz.msgs.IMU
frame is robotmodel/imu/imu-sensor

**lidar**
/lidar : gz.msgs.LaserScan
frame is robotmodel/robot-lidar/robot-lidar

**wheel odometry**
/model/robot-model/odometry : gz.msgs.Odometry
frame is robotmodel/odom
child frame is robotmodel/robot-base

I guess this also publishes a tf between robotmodel/odom and robotmodel/robot-base frames

/model/robot-model/tf : gz.msgs.Pose_V


/model/robot-model/cmd_vel : gz.msgs.Twist





