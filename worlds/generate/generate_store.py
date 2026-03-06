import csv

sdf_store_str_head = """<?xml version="1.0" ?>
<sdf version="1.7">
  <world name="default">

    <!-- Physics -->
    <physics name="default_physics" type="ode">
      <max_step_size>0.001</max_step_size>
      <real_time_factor>1.0</real_time_factor>
      <real_time_update_rate>1000</real_time_update_rate>
    </physics>

    <!-- Lighting -->
    <light type="directional" name="sun">
      <cast_shadows>true</cast_shadows>
      <pose>0 0 10 0 0 0</pose>
      <diffuse>0.8 0.8 0.8 1</diffuse>
      <specular>0.2 0.2 0.2 1</specular>
      <direction>-0.5 0.5 -1</direction>
    </light>

    <!-- Ground plane -->
	<include>
		<uri>model://floor</uri>
	</include>

	<include>
		<uri>file://robot.sdf</uri>
		<pose degrees='true'>7 1 0.1 0 0 0</pose>
	</include>

    <model name="bench">
        <static>true</static>
        <pose>7.75 3 1 0 0 0</pose>
        <link name="bench-link">
          <visual name="bench-visual">
            <geometry>
              <box>
                <size>0.5 6 2</size>
              </box>
            </geometry>
            </visual>
          <collision name="bench-collision">
            <geometry>
              <box>
                <size>0.5 6 2</size>
              </box>
            </geometry>
          </collision>
        </link>
    </model>

    <model name="wall-bottom">
        <static>true</static>
        <pose>4.75 -0.1 1 0 0 0</pose>
        <link name="link">
          <visual name="visual">
            <geometry>
              <box>
                <size>9.5 0.2 2</size>
              </box>
            </geometry>
          </visual>
          <collision name="collision">
            <geometry>
              <box>
                <size>9.5 0.2 2</size>
              </box>
            </geometry>
          </collision>
        </link>
    </model>

    <model name="wall-top">
        <static>true</static>
        <pose>4.75 6.1 1 0 0 0</pose>
        <link name="link">
          <visual name="visual">
            <geometry>
              <box>
                <size>9.5 0.2 2</size>
              </box>
            </geometry>
          </visual>
          <collision name="collision">
            <geometry>
              <box>
                <size>9.5 0.2 2</size>
              </box>
            </geometry>
          </collision>
        </link>
    </model>

    <model name="wall-left">
        <static>true</static>
        <pose>-0.1 3 1 0 0 0</pose>
        <link name="link">
          <visual name="visual">
            <geometry>
              <box>
                <size>0.2 6 2</size>
              </box>
            </geometry>
          </visual>
          <collision name="collision">
            <geometry>
              <box>
                <size>0.2 6 2</size>
              </box>
            </geometry>
          </collision>
        </link>
    </model>

    <model name="wall-right">
        <static>true</static>
        <pose>9.6 3 1 0 0 0</pose>
        <link name="link">
          <visual name="visual">
            <geometry>
              <box>
                <size>0.2 6 2</size>
              </box>
            </geometry>
          </visual>
          <collision name="collision">
            <geometry>
              <box>
                <size>0.2 6 2</size>
              </box>
            </geometry>
          </collision>
        </link>
    </model>



"""

sdf_store_str_tail = """
  </world>
</sdf>"""

if __name__=="__main__":
    shelf_poses = []

    ## Bottom Shelves
    for i in range(0,6):
        shelf_poses.append((0.5+i,0,0,0,0,0))

    ## Middle Shelves
    for i in range(0,5):
        shelf_poses.append((2.5+i,2,0,0,0,180))
        shelf_poses.append((1.5+i,2,0,0,0,0))
        shelf_poses.append((2.5+i,4,0,0,0,180))
        shelf_poses.append((1.5+i,4,0,0,0,0))

    ## Top Shelves
    for i in range(0,7):
        shelf_poses.append((1.5+i,6,0,0,0,180))

    ## Left Shelves
    for i in range(0,6):
        shelf_poses.append((0,1+i,0,0,0,-90))

    ## Generate sdf
    out_str = sdf_store_str_head

    for i,p in enumerate(shelf_poses):

        out_str += f"""<include>
      <uri>model://shelf2</uri>
      <name>shelf_{i}</name>
      <static>true</static>
      <pose degrees="true">{p[0]} {p[1]} {p[2]} {p[3]} {p[4]} {p[5]}</pose>
</include>"""

    out_str += sdf_store_str_tail


    ## Create the sdf model
    with open('./formatted_out.sdf','w') as wfile:
        wfile.write(out_str)

    ## Generate a config JSON with shelf (RFID tag) locations
    ## Each shelf will have four shelves, each with three free spots

    open_spots = []

    for spose in shelf_poses:
        ## Append 12-free spots

        for i in range(0,4):
            if spose[-1] == 180:
                open_spots.append((spose[0]-0.25, spose[1]-0.25,0.3+i*0.3))
                open_spots.append((spose[0]-0.5, spose[1]-0.25,0.3+i*0.3))
                open_spots.append((spose[0]-0.75, spose[1]-0.25,0.3+i*0.3))
            elif spose[-1] == 90:
                open_spots.append((spose[0]-0.25, spose[1]+0.25,0.3+i*0.3))
                open_spots.append((spose[0]-0.25, spose[1]+0.5,0.3+i*0.3))
                open_spots.append((spose[0]-0.25, spose[1]+0.75,0.3+i*0.3))
            elif spose[-1] == -90:
                open_spots.append((spose[0]+0.25, spose[1]-0.25,0.3+i*0.3))
                open_spots.append((spose[0]+0.25, spose[1]-0.5,0.3+i*0.3))
                open_spots.append((spose[0]+0.25, spose[1]-0.75,0.3+i*0.3))
            elif spose[-1] == 0:
                ## Default
                open_spots.append((spose[0]+0.25, spose[1]+0.25,0.3+i*0.3))
                open_spots.append((spose[0]+0.5, spose[1]+0.25,0.3+i*0.3))
                open_spots.append((spose[0]+0.75, spose[1]+0.25,0.3+i*0.3))
            else:
                print("Bad z-axis rotation for shelf-poses : set in range [-90,180]")

    with open('./tag_locs.txt','w') as wfile2:

        cs = csv.writer(wfile2)

        for s in open_spots:
            cs.writerow(s)






