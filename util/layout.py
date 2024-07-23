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
	def __init__(self, num, height, width, depth):
		''' Generates an sdf object representing a shelf
		@param num: The unique identifier of this shelf
		@param height: the height (z-axis) of this shelf
		@param width: the width (y-axis) of this shelf
		@param depth: the depth (x-axis) of this shelf
		@return: The full SDF file of this shelf
		'''

		#NOTE: change this to class method to keep track of how many shelfs are there
		self.num = num

		self.height = height
		self.width = width
		self.depth = depth

		self.support_width = 0.1
		self.shelf_depth_flat = 0.1

		self.support_links = 0
		self.shelf_links = 0

		#NOTE: The shelf should have four support links (one on each corner) and three shelfs.
		return

	def generatePoses(self):
		''' Generates a dictionary of all shelf and support poses based on h/w/d
		@return: dictionary of shelf/support poses
		'''
		poses = {}
		poses['support'] = [f'{self.support_width/2} {self.support_width/2} {self.height/2} 0 0 0',
							f'{self.depth-self.support_width/2} {self.support_width/2} {self.height/2} 0 0 0',
							f'{self.support_width/2} {self.width-self.support_width/2} {self.height/2} 0 0 0',
							f'{self.depth-self.support_width/2} {self.width-self.support_width/2} {self.height/2} 0 0 0']
		poses['shelf'] = [f'{self.depth/2} {self.width/2} {self.height-self.shelf_depth_flat/2} 0 0 0',
							f'{self.depth/2} {self.width/2} {self.height*2/3-self.shelf_depth_flat/2} 0 0 0',
							f'{self.depth/2} {self.width/2} {self.height/3-self.shelf_depth_flat/2} 0 0 0']
		
		return poses

	def construct(self):
		"""	Returns 
		@return: The Shelf Element object, beginning with the <link> tag
		"""

		## Boilerplate
		root = ET.Element('sdf', attrib={'version': '1.11'})
		model = ET.SubElement(root, 'model', attrib={'name': f'store-shelf-{self.num}'})
		static = ET.SubElement(model, 'static')
		static.text = 'true'

		poselist = self.generatePoses()

		for p in poselist['shelf']:
			self.addShelfLink(model, p)

		for p in poselist['support']:
			self.addSupportLink(model, p)
		
		if (True):
			ET.indent(root, space=' ')
			print(ET.tostring(root, encoding='unicode'))

		return root

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
		addElem('size', box, text=f'{self.depth} {self.width} {self.shelf_depth_flat}')
		return

	def addSupportGeometry(self, gparent):
		box = ET.SubElement(gparent, 'box')
		addElem('size', box, text=f'{self.support_width} {self.support_width} {self.height}')
		return

class StoreWalls:
	
	def __init(self, num, width, depth, height):
		self.num = num
		self.wallnum = 0

		# y
		self.width = width
		# x
		self.depth = depth
		# z
		self.height = height

	def construct(self):
		"""	Returns 
		@return: The Shelf Element object, beginning with the <link> tag
		"""

		## Boilerplate
		root = ET.Element('sdf', attrib={'version': '1.11'})
		model = ET.SubElement(root, 'model', attrib={'name': f'store-walls-{self.num}'})
		static = ET.SubElement(model, 'static')
		static.text = 'true'

		if (True):
			ET.indent(root, space=' ')
			print(ET.tostring(root, encoding='unicode'))

		for p in ['(']:
			wall = addWall(p, model)

		return root
	
	def addWall(self, pose, parent):
		lk = ET.SubElement(parent, 'link', attrib={'name': f'store-walls-link{self.wallnum}'})
		##NOTE: need to update this
		addElem('pose',lk, text='0 0 0 0 0 0')
		
		## Add link visual
		visu = addElem('visual', lk, attrib={'name':f'store-walls-v{self.num}-wall{self.wallnum}'})
		geom = addElem('geometry', visu)
		box = ET.SubElement(geom, 'box')
		addElem('size', box, text=f'{self.depth} {self.width} {self.height}')
		
		mate = ET.SubElement(visu, 'material')
		addElem('diffuse', mate, text='0.726 0.726 0.726 1')
		addElem('ambient', mate, text='0.726 0.726 0.726 1')
		addElem('specular', mate, text='0.726 0.726 0.726 1')

		## Add link collision
		coll = addElem('collision', lk, attrib={'name':f'store-shelf-c{self.num}-shelf{self.shelf_links}'})
		geom = addElem('geometry', coll)
		box = ET.SubElement(geom, 'box')
		addElem('size', box, text=f'{self.depth} {self.width} {self.height}')


		self.wallnum += 1
		return
	

if __name__=="__main__":
	
	a = StoreShelf(0, 1, 1, 0.4)
	a.construct()
	
	'''
	outname = 'storelayout.sdf'
	with open(f'../models/{outname}','w') as wfile:
		wfile.write(ET.tostring(root, encoding='unicode'))
	'''

