import csv

sdf_store_str_head = """<?xml version="1.0" ?>
<sdf version="1.7">
  <model name="store_layout">

    <!-- Ground plane -->
    <model name="floor">
		<static>true</static>
		<pose>7 5 0 0 0 0</pose>
      <link name="link">

        <collision name="collision">
          <geometry>
            <plane>
              <normal>0 0 1</normal>
              <size>14 10</size>
            </plane>
          </geometry>
        </collision>

        <visual name="visual">
          <geometry>
            <plane>
              <normal>0 0 1</normal>
              <size>14 10</size>
            </plane>
			</geometry>

			<material>
				<ambient>1 1 1 1</ambient>
          		<diffuse>1 1 1 1</diffuse>
          		<specular>0.1 0.1 0.1 1</specular>
			</material>
        </visual>
      </link>
    </model>

	<include>
		<uri>model://robot</uri>
		<pose degrees='true'>10 1 0.1 0 0 0</pose>
	</include>
"""

sdf_store_str_tail = """
  </model>
</sdf>"""

light_str = """
<model name="{name}">
    <static>true</static>
    <link name="link">
    <light name="light" type="spot">
      <pose>{x} {y} {z} {phi} {theta} {psi}</pose>
      <cast_shadows>false</cast_shadows>
      <diffuse>1.0 0.98 0.94 1.0</diffuse>
      <specular>0.15 0.15 0.15 1.0</specular>

      <direction>0 0 -1</direction>

      <attenuation>
        <range>10.0</range>
        <constant>0.9</constant>
        <linear>0.1</linear>
        <quadratic>0.02</quadratic>
      </attenuation>

      <spot>
        <inner_angle>0.65</inner_angle>
        <outer_angle>1.0</outer_angle>
        <falloff>0.8</falloff>
      </spot>
    </light>
</link>
</model>
"""

wall_str ='''<model name="{name}">
        <static>true</static>
        <pose>{x} {y} {z} {phi} {theta} {psi}</pose>
        <link name="link">
          <visual name="visual">
            <geometry>
              <box>
                <size>{width} {depth} {height}</size>
              </box>
            </geometry>
          </visual>
          <collision name="collision">
            <geometry>
              <box>
                <size>{width} {depth} {height}</size>
              </box>
            </geometry>
          </collision>
        </link>
    </model>
'''

def generate_wall(name, x, y, z, phi, theta, psi, width, depth, height):
    return wall_str.format(name=name,x=x,y=y,z=z,phi=phi,theta=theta,psi=psi,width=width,depth=depth,height=height)

def generate_light(name, x, y, z, phi, theta, psi):
    return light_str.format(name=name, x=x, y=y, z=z, phi=phi, theta=theta, psi=psi)

if __name__=="__main__":


    shelf_poses = []

    ## Bottom Shelves
    for i in range(0,9):
        shelf_poses.append((0.5+i,0,0,0,0,0))

    ## Middle Shelves
    for i in range(0,7):
        shelf_poses.append((3.5+i,2.5,0,0,0,180))
        shelf_poses.append((2.5+i,2.5,0,0,0,0))

        shelf_poses.append((3.5+i,5,0,0,0,180))
        shelf_poses.append((2.5+i,5,0,0,0,0))

        shelf_poses.append((3.5+i,7.5,0,0,0,180))
        shelf_poses.append((2.5+i,7.5,0,0,0,0))

    ## Top Shelves
    for i in range(0,11):
        shelf_poses.append((1.5+i,10,0,0,0,180))

    ## Left Shelves
    for i in range(0,10):
        shelf_poses.append((0,1+i,0,0,0,-90))

    ## Right Shelves
    for i in range(0,5):
        shelf_poses.append((12,5+i,0,0,0,90))

    ## End Shelves
        shelf_poses.append((2.5,2,0,0,0,90))
        shelf_poses.append((2.5,4.5,0,0,0,90))
        shelf_poses.append((2.5,7,0,0,0,90))

        shelf_poses.append((9.5,3,0,0,0,-90))
        shelf_poses.append((9.5,5.5,0,0,0,-90))
        shelf_poses.append((9.5,8,0,0,0,-90))


    ## Generate sdf
    out_str = sdf_store_str_head

    for i,p in enumerate(shelf_poses):

        out_str += f"""<include>
      <uri>model://shelf</uri>
      <name>shelf_{i}</name>
      <static>true</static>
      <pose degrees="true">{p[0]} {p[1]} {p[2]} {p[3]} {p[4]} {p[5]}</pose>
</include>"""

    ## Add Walls
    out_str += generate_wall('wall-left', -0.1, 5, 1.5, 0, 0, 0, 0.2, 10, 3)
    out_str += generate_wall('wall-top', 7, 10.1, 1.5, 0, 0, 0, 14, 0.2, 3)
    out_str += generate_wall('wall-bottom', 7, -0.1, 1.5, 0, 0, 0, 14, 0.2, 3)
    out_str += generate_wall('wall-right', 14.1, 5, 1.5, 0, 0, 0, 0.2, 10, 3)
    out_str += generate_wall('wall-bench-top', 12.1, 7.5, 1.5, 0, 0, 0, 0.2, 5, 3)
    out_str += generate_wall('wall-bench-bottom', 12.1, 0.5, 1.5, 0, 0, 0, 0.2, 1, 3)

    ## Add lights
    for i in range(0, 8):
        out_str += generate_light(f'light-a-{i}', 1.25, 1.5+i, 3, 0, 0, 0)
        out_str += generate_light(f'light-c-{i}', 10.75, 1.5+i, 3, 0, 0, 0)
        out_str += generate_light(f'light-d-{i}', 13, 1.5+i, 3, 0, 0, 0)

    for i in range(0, 8):
        out_str += generate_light(f'light-r0-{i}', 2.5+i, 8.75, 3, 0, 0, 0)
        out_str += generate_light(f'light-r1-{i}', 2.5+i, 6.25, 3, 0, 0, 0)
        out_str += generate_light(f'light-r2-{i}', 2.5+i, 3.75, 3, 0, 0, 0)
        out_str += generate_light(f'light-r3-{i}', 2.5+i, 1.25, 3, 0, 0, 0)



    out_str += sdf_store_str_tail

    ## Create the sdf model
    with open('./out_generated.sdf','w') as wfile:
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






