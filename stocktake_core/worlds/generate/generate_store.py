import csv
import random
import sys

sdf_store_str_head = """<?xml version="1.0" ?>
<sdf version="1.7">
  <model name="store_layout">

    <include>
        <uri>model://floor</uri>
        <name>floor</name>
        <static>true</static>
    </include>

    <include>
        <uri>model://ceiling</uri>
        <name>ceiling</name>
        <static>true</static>
    </include>

    <!-- Robot type -->
	<include>
		<uri>model://{}</uri>
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
      <diffuse>0.4 0.39 0.35 1.0</diffuse>
      <specular>0.02 0.02 0.02 1.0</specular>

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
    
    #TODO Change to argparse
    print("Generating store")

    if (len(sys.argv)!=4):
        print("Store generation: not enough arguments")
        exit()

    if (sys.argv[1] not in ['lidar', 'rgbd', 'stereo', 'mono']):
        print("Store generation: bad robot_type arg")
        exit()

    robot_type = sys.argv[1]
    output_file = sys.argv[2]
    seed = sys.argv[3]

    ## Start generation
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
        #shelf_poses.append((1.5+i,10,0,0,0,180))
        pass

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
    if robot_type=='lidar':
        robot_model_filename='robot-lidar'
    elif robot_type=='rgbd':
        robot_model_filename='robot-rgbd'
    elif robot_type=='stereo':
        robot_model_filename='robot-stereo'
    elif robot_type=='mono':
        robot_model_filename='robot-mono'

    out_str = sdf_store_str_head.format(robot_model_filename)

    for i,p in enumerate(shelf_poses):
        out_str += f"""<include>
      <uri>model://shelf-a</uri>
      <name>shelf_{i}</name>
      <static>true</static>
      <pose degrees="true">{p[0]} {p[1]} {p[2]} {p[3]} {p[4]} {p[5]}</pose>
</include>"""

    ## Add Walls
    '''
    out_str += generate_wall('wall-left', -0.1, 5, 1.5, 0, 0, 0, 0.2, 10, 3)
    out_str += generate_wall('wall-top', 7, 10.1, 1.5, 0, 0, 0, 14, 0.2, 3)
    out_str += generate_wall('wall-bottom', 7, -0.1, 1.5, 0, 0, 0, 14, 0.2, 3)
    out_str += generate_wall('wall-right', 14.1, 5, 1.5, 0, 0, 0, 0.2, 10, 3)
    out_str += generate_wall('wall-bench-top', 12.1, 7.5, 1.5, 0, 0, 0, 0.2, 5, 3)
    out_str += generate_wall('wall-bench-bottom', 12.1, 0.5, 1.5, 0, 0, 0, 0.2, 1, 3)
    '''

    out_str += """<include>
    <uri>model://wall-east</uri>
    <name>wall_east</name>
    <static>true</static>
</include>
<include>
    <uri>model://wall-south</uri>
    <name>wall_south</name>
    <static>true</static>
</include>
<include>
    <uri>model://wall-north</uri>
    <name>wall_north</name>
    <static>true</static>
</include>"""

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



    ## Generate a config JSON with shelf (RFID tag) locations
    ## Each shelf will have four shelves, each with three free spots

    open_spots = []

    for spose in shelf_poses:
        ## Append 12-free spots

        for i in range(0,4):
            if spose[-1] == 180:
                open_spots.append((spose[0]-0.25, spose[1]-0.25,0.3+i*0.5))
                open_spots.append((spose[0]-0.5, spose[1]-0.25,0.3+i*0.5))
                open_spots.append((spose[0]-0.75, spose[1]-0.25,0.3+i*0.5))
            elif spose[-1] == 90:
                open_spots.append((spose[0]-0.25, spose[1]+0.25,0.3+i*0.5))
                open_spots.append((spose[0]-0.25, spose[1]+0.5,0.3+i*0.5))
                open_spots.append((spose[0]-0.25, spose[1]+0.75,0.3+i*0.5))
            elif spose[-1] == -90:
                open_spots.append((spose[0]+0.25, spose[1]-0.25,0.3+i*0.5))
                open_spots.append((spose[0]+0.25, spose[1]-0.5,0.3+i*0.5))
                open_spots.append((spose[0]+0.25, spose[1]-0.75,0.3+i*0.5))
            elif spose[-1] == 0:
                ## Default
                open_spots.append((spose[0]+0.25, spose[1]+0.25,0.3+i*0.5))
                open_spots.append((spose[0]+0.5, spose[1]+0.25,0.3+i*0.5))
                open_spots.append((spose[0]+0.75, spose[1]+0.25,0.3+i*0.5))
            else:
                print("Bad z-axis rotation for shelf-poses : set in range [-90,180]")

    
    for i,p in enumerate(random.sample(open_spots, k=200)):
        out_str += f"""<include>
            <uri>model://cereal{1 if i%2 else 2}</uri>
            <name>cereal_{i}</name>
            <static>true</static>
            <pose degrees="true">{p[0]} {p[1]} {p[2]} 0 0 0</pose>
        </include>"""

    ## add counter
    out_str += f"""<include>
    <uri>model://counter</uri>
    <name>counter</name>
    <static>true</static>
    <pose degrees="true">11.5 1 0 0 0 0</pose>
    </include>"""

    ## NOTE Temporarily disabling writing of output tag locations
    '''
    with open('./tag_locs.txt','w') as wfile2:
        cs = csv.writer(wfile2)
        for s in open_spots:
            cs.writerow(s)
    '''

    out_str += sdf_store_str_tail

    ## Create the sdf model
    # NOTE The 'output_file' variable is passed through as a launch argument and contains the path to the generated world
    # sdf file in the **install** directory, not the **src** directory. If we built with --symlink-install, it will 
    # overwrite the symlink, but this is not an issue.
    print(f"Store generation: writing to {output_file}")
    with open(f'{output_file}','w') as wfile:
        wfile.write(out_str)


