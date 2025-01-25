#!/usr/bin/python3

'''
Script to procedurally generate the store layout


<sdf>
	<model>
		<link>
			<pose>
			<visual>
			<collision>

'''

import os
import numpy as np
import xml.etree.ElementTree as ET

def addElem(name, parent, text=None, attrib=None):
	if attrib is None:
		elem = ET.SubElement(parent, name)
	else:
		elem = ET.SubElement(parent, name, attrib=attrib)
		
	if text is not None:
		elem.text = text
	return elem

class StoreShelf:
	shelf_num=0
	def __init__(self, model_node, loc=(0,0,0), dims=(1,1,1), table=False):
		''' Generates an sdf object representing a shelf
    @param model_node: The model element that these <link> tags will be created under
		@param loc: An (x,y,z) tuple representing the location of the shelf. Shelf corner will be located at (x,y,z) and extended out in positive x,y,z axes by width, depth, height respectively.
		@param dims: A tuple representing the (width, depth, height) of the Shelf, along the x,y,z axes respectively.
		'''
		self.model = model_node

		self.num = StoreShelf.shelf_num
		StoreShelf.shelf_num += 1

		self.x, self.y, self.z = loc

		self.width, self.depth, self.height = dims

		## Shelf specific configurations
		self.support_width = 0.1
		self.shelf_depth_flat = 0.1

		self.support_links = 0
		self.shelf_links = 0

		self.table = table

		#NOTE: The shelf should have four support links (one on each corner) and three shelfs.
		return

	def generatePoses(self):
		''' Generates a dictionary of all shelf and support poses based on h/w/d
		@return: dictionary of shelf/support poses
		'''
		poses = {}
		poses['support'] = [
							f'{self.x + self.support_width/2} {self.y + self.support_width/2} {self.z + self.height/2} 0 0 0',
							f'{self.x + self.width-self.support_width/2} {self.y + self.support_width/2} {self.z + self.height/2} 0 0 0',
							f'{self.x + self.support_width/2} {self.y + self.depth-self.support_width/2} {self.z + self.height/2} 0 0 0',
							f'{self.x + self.width-self.support_width/2} {self.y + self.depth-self.support_width/2} {self.z + self.height/2} 0 0 0']
		poses['shelf'] = [f'{self.x + self.width/2} {self.y + self.depth/2} {self.z + self.height-self.shelf_depth_flat/2} 0 0 0',
							f'{self.x + self.width/2} {self.y + self.depth/2} {self.z + self.height*2/3-self.shelf_depth_flat/2} 0 0 0',
							f'{self.x + self.width/2} {self.y + self.depth/2} {self.z + self.height/3-self.shelf_depth_flat/2} 0 0 0']
		
		return poses

	def construct(self):
		"""	Returns 
		@return: The Shelf Element object, beginning with the <link> tag
		"""

		## Boilerplate
		#model = ET.SubElement(self.root, 'model', attrib={'name': f'store-shelf-{self.num}'})
		#static = ET.SubElement(model, 'static')
		#static.text = 'true'

		poselist = self.generatePoses()

		if self.table:
			## Only generate top shelf if this is a table
			self.addShelfLink(self.model, poselist['shelf'][0])
		else:
			for p in poselist['shelf']:
				self.addShelfLink(self.model, p)

		for p in poselist['support']:
			self.addSupportLink(self.model, p)

		return

	def addSupportLink(self, model_parent, pose):
		lk = ET.SubElement(model_parent, 'link', attrib={'name':f'store-shelf-l{self.num}-support{self.support_links}'})
		
		## Add link pose
		addElem('pose', lk, text=pose)
		
		## Add link visual
		visu = addElem('visual', lk, attrib={'name':f'store-shelf-v{self.num}-support{self.support_links}'})
		geom = addElem('geometry', visu)
		self.addSupportGeometry(geom)
		
		mate = ET.SubElement(visu, 'material')
		addElem('diffuse', mate, text='0.726 0.726 0.726 1')
		addElem('ambient', mate, text='0.726 0.726 0.726 1')
		addElem('specular', mate, text='0.726 0.726 0.726 1')

		## Add link collision
		coll = addElem('collision', lk, attrib={'name':f'store-shelf-c{self.num}-support{self.support_links}'})
		geom = addElem('geometry', coll)
		self.addSupportGeometry(geom)
		
		self.support_links += 1

		return

	def addShelfLink(self, model_parent, pose):
		lk = ET.SubElement(model_parent, 'link', attrib={'name':f'store-shelf-l{self.num}-shelf{self.shelf_links}'})
		
		## Add link pose
		addElem('pose', lk, text=pose)
		
		## Add link visual
		visu = addElem('visual', lk, attrib={'name':f'store-shelf-v{self.num}-shelf{self.shelf_links}'})
		geom = addElem('geometry', visu)
		self.addShelfGeometry(geom)
		
		mate = ET.SubElement(visu, 'material')
		addElem('diffuse', mate, text='0.726 0.726 0.726 1')
		addElem('ambient', mate, text='0.726 0.726 0.726 1')
		addElem('specular', mate, text='0.726 0.726 0.726 1')

		## Add link collision
		coll = addElem('collision', lk, attrib={'name':f'store-shelf-c{self.num}-shelf{self.shelf_links}'})
		geom = addElem('geometry', coll)
		self.addShelfGeometry(geom)
		
		self.shelf_links += 1

		return

		
	## NOTE: Should probably condense these two into one
	def addShelfGeometry(self, gparent):
		box = ET.SubElement(gparent, 'box')
		addElem('size', box, text=f'{self.width} {self.depth} {self.shelf_depth_flat}')
		return

	def addSupportGeometry(self, gparent):
		box = ET.SubElement(gparent, 'box')
		addElem('size', box, text=f'{self.support_width} {self.support_width} {self.height}')
		return

class StoreWalls:
	num_walls = 0 

	def __init__(self, model_node, loc=(0,0,0), dims=(1,1,1)):
		self.model = model_node

    ## Unpack location
		self.x, self.y, self.z = loc

    ## Unpack dimensions
    ## width is along x-axis, depth is along y-axis, height is along z-axis
		self.width, self.depth, self.height = dims

	def construct(self):
		"""	Returns 
		@return: The Shelf Element object, beginning with the <link> tag
		"""

		## Boilerplate
		#model = ET.SubElement(self.root, 'model', attrib={'name': f'store-walls-{self.num}'})
		#static = ET.SubElement(model, 'static')
		#static.text = 'true'

		for p in [f'{self.x + self.width/2} {self.y + self.depth/2} {self.z + self.height/2} 0 0 0']:
			wall = self.addWall(p, self.model)

		return
	
	def addWall(self, pose, parent):
		lk = ET.SubElement(parent, 'link', attrib={'name': f'store-walls-link{StoreWalls.num_walls}'})
		##NOTE: need to update this
		addElem('pose',lk, text=pose)
		
		## Add link visual
		visu = addElem('visual', lk, attrib={'name':f'store-walls-v-wall{StoreWalls.num_walls}'})
		geom = addElem('geometry', visu)
		box = ET.SubElement(geom, 'box')
		addElem('size', box, text=f'{self.width} {self.depth} {self.height}')
		
		mate = ET.SubElement(visu, 'material')
		addElem('diffuse', mate, text='0.726 0.726 0.726 1')
		addElem('ambient', mate, text='0.726 0.726 0.726 1')
		addElem('specular', mate, text='0.726 0.726 0.726 1')

		## Add link collision
		coll = addElem('collision', lk, attrib={'name':f'store-walls-c-wall{StoreWalls.num_walls}'})
		geom = addElem('geometry', coll)
		box = ET.SubElement(geom, 'box')
		addElem('size', box, text=f'{self.width} {self.depth} {self.height}')

		## Increment total walls counter
		StoreWalls.num_walls += 1

		return

class RoundTable:
	rt_num=0
	def __init__(self, model_node, loc=(0,0,0), dims=(1,1,1)):
		self.model = model_node
		
		self.x, self.y, self.z = loc
		self.width, self.depth, self.height = dims

	def construct(self):
		## Base has height 0.14 but table top has height 0.1
		self.addCyl(0.4, 0.07, 0.14)
		self.addCyl(0.1, self.height/2, self.height)
		self.addCyl(0.7, self.height-0.05, 0.1)

	def addCyl(self, rad, height, length):
		lk = ET.SubElement(self.model, 'link', attrib={'name': f'store-rt-link{RoundTable.rt_num}'})
		addElem('pose',lk, text=(f'{self.x} {self.y} {height} 0 0 0'))
		
		## Add link visual
		visu = addElem('visual', lk, attrib={'name':f'store-rt-v{RoundTable.rt_num}'})
		geom = addElem('geometry', visu)
		cyl = ET.SubElement(geom, 'cylinder')
		addElem('radius', cyl, text=f'{rad}')
		addElem('length', cyl, text=f'{length}')
		
		mate = ET.SubElement(visu, 'material')
		addElem('diffuse', mate, text='0.726 0.726 0.726 1')
		addElem('ambient', mate, text='0.726 0.726 0.726 1')
		addElem('specular', mate, text='0.726 0.726 0.726 1')

		## Add link collision
		coll = addElem('collision', lk, attrib={'name':f'store-rt-c{RoundTable.rt_num}'})
		geom = addElem('geometry', coll)
		cyl = ET.SubElement(geom, 'cylinder')
		addElem('radius', cyl, text=f'{rad}')
		addElem('length', cyl, text=f'{length}')

		## Increment counter
		RoundTable.rt_num += 1

		return

if __name__=="__main__":
  ### Generate a store layout in the <model> tag

	## Boilerplate
	root = ET.Element('sdf', attrib={'version': '1.11'})
	model = ET.SubElement(root, 'model', attrib={'name': f'store-model'})
	static = ET.SubElement(model, 'static')
	static.text = 'true'

	## Configuration
	
	## Add Shelf
	StoreShelf(model, loc=(2, 9, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(2, 8, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(2, 7, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(3, 7, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(3, 8, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(3, 9, 0), dims=(1, 0.4, 1)).construct()

	StoreShelf(model, loc=(-1, 7, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(-1, 8, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(-1, 9, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(0, 7, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(0, 8, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(0, 9, 0), dims=(1, 0.4, 1)).construct()

	StoreShelf(model, loc=(-4, 7, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(-4, 8, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(-4, 9, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(-3, 7, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(-3, 8, 0), dims=(1, 0.4, 1)).construct()
	StoreShelf(model, loc=(-3, 9, 0), dims=(1, 0.4, 1)).construct()

	## Tables
	StoreShelf(model, loc=(-4, 0, 0), dims=(1, 2, 1), table=True).construct()
	StoreShelf(model, loc=(-4, -4, 0), dims=(1, 2, 1), table=True).construct()
	StoreShelf(model, loc=(0, 1, 0), dims=(2, 1, 1), table=True).construct()

	## Walls
	StoreWalls(model, loc=(5, -10, 0), dims=(0.2, 20, 2)).construct()
	StoreWalls(model, loc=(-5.2, -10, 0), dims=(0.2, 20, 2)).construct()
	StoreWalls(model, loc=(-5.2, 10, 0), dims=(10.4, 0.2, 2)).construct()
	StoreWalls(model, loc=(-5.2, -10.2, 0), dims=(10.4, 0.2, 2)).construct()

	## Round Tables
	RoundTable(model, loc=(3,-4,0), dims=(-1,-1, 1)).construct()
	RoundTable(model, loc=(2,-7,0), dims=(-1,-1, 1)).construct()
	RoundTable(model, loc=(-3,-7,0), dims=(-1,-1, 1)).construct()
	RoundTable(model, loc=(-1,-3,0), dims=(-1,-1, 1)).construct()
	
	outname = 'storelayout.sdf'
	if os.getcwd().split('/')[-1] == 'util':
		## Running from util/
		outpath = f'../models/{outname}'
	else:
		## Running from stocktake/
		outpath = f'./models/{outname}'

	if False:
		## NOTE: Is this being output via shell redirection in run.sh or are we writing directly to file
		with open(outpath,'w') as wfile:
			wfile.write(ET.tostring(root, encoding='unicode'))

	## Print output
	ET.indent(root, space=' ')
	print(ET.tostring(root, encoding='unicode'))

