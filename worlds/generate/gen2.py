import csv
import random

sdf_store_str_head = """<?xml version="1.0" ?>
<sdf version="1.7">
  <model name="store-layout">

    <model name="floor">
		<static>true</static>
	<pose>25 10 0 0 0 0</pose>
      <link name="link">

        <collision name="collision">
          <geometry>
            <plane>
              <normal>0 0 1</normal>
              <size>50 20</size>
            </plane>
          </geometry>
        </collision>

        <visual name="visual">
          <geometry>
            <plane>
              <normal>0 0 1</normal>
              <size>50 20</size>
            </plane>
			</geometry>

			<material>
				<ambient>1 1 1 1</ambient>
          		<diffuse>1 1 1 1</diffuse>
          		<specular>0.1 0.1 0.1 1</specular>

          <pbr>
            <metal>
              <albedo_map>model://floor/materials/textures/concrete-floor.jpg</albedo_map>
              <roughness>0.9</roughness>
              <metalness>0.5</metalness>
            </metal>
          </pbr>
			</material>


        </visual>

      </link>
    </model>

	<include>
		<uri>model://robot</uri>
		<pose degrees='true'>0.5 0.5 0.1 0 0 0</pose>
	</include>

"""

sdf_store_str_tail = """
  </model>
</sdf>"""

if __name__=="__main__":
    # TODO Pass these as CLAs

    ## Store dimensions
    dims = (50,20)

    seed = 'aaa'
    random.seed(seed)
    
    p = 0.4

    ## Algorithm
    '''Our shelf is 1mx0.5m, and so for every square in our store, we first decide to spawn a shelf
    with probability p and then we choose on of four different orientations within the grid. While
    this algorithm doesn't really give us a realistic store, it gives us an environment which we can
    conduct 2D SLAM reasonably well.'''

    shelf_poses = []


    for i in range(0,dims[0]):
        for j in range(0, dims[1]):
            ## Ignore cell (0,0) as that is where robot starts
            if i==0 and j==0:
                continue

            if random.random() <= 0.4:
                o = random.random()
                if o<=0.25:
                    shelf_poses.append((i,j,0,0,0,0))
                elif o<= 0.5:
                    shelf_poses.append((i+1,j+1,0,0,0,180))
                    pass
                elif o<= 0.75:
                    shelf_poses.append((i+1,j,0,0,0,90))
                else:
                    shelf_poses.append((i,j+1,0,0,0,-90))

    ## Generate sdf
    out_str = sdf_store_str_head

    for i,p in enumerate(shelf_poses):

        out_str += f"""<include>
      <uri>model://shelf</uri>
      <name>shelf_{i}</name>
      <static>true</static>
      <pose degrees="true">{p[0]} {p[1]} {p[2]} {p[3]} {p[4]} {p[5]}</pose>
</include>"""

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






