#!/usr/bin/python3

'''
Python script to determine the correct angle of the mecanum wheel

Should be configurable for an arbitrary number of mecanum rollers

See mecanum wheel sketch for more detail
'''

import numpy as np
import xml.etree.ElementTree as ET

class MecanumJoint:
	def __init__(self, num):
		self.num = num

	def construct(self):
		root = ET.Element('joint', attrib={'name':f'mw-j{self.num}', 'type':'revolute'})
		pose = ET.SubElement(root, 'pose')
		pose.text = '0 0 0 0 0 0'
		parent = ET.SubElement(root, 'parent')
		parent.text = 'mecanum-base'
		child = ET.SubElement(root, 'child')
		child.text = f'mw-l{self.num}'
		
		axis = ET.SubElement(root, 'axis')
		xyz = ET.SubElement(axis, 'xyz')
		xyz.text = '0 1 0'
		limit = ET.SubElement(axis, 'limit')
		lower = ET.SubElement(limit, 'lower')
		lower.text = '-1.79769e+308'
		upper = ET.SubElement(limit, 'upper')
		upper.text = '1.79769e+308'
		effort = ET.SubElement(limit, 'effort')
		effort.text = '-1'
		velocity = ET.SubElement(limit, 'velocity')
		velocity.text = '-1'

		dynamics = ET.SubElement(axis, 'dynamics')
		spring_ref = ET.SubElement(dynamics, 'spring_reference')
		spring_ref.text = '0'
		spring_stiff = ET.SubElement(dynamics, 'spring_stiffness')
		spring_stiff.text = '0'
		damping = ET.SubElement(dynamics, 'damping')
		damping.text = '0'
		friction = ET.SubElement(dynamics, 'friction')
		friction.text = '0'

		physics = ET.SubElement(root, 'physics')
		ode = ET.SubElement(physics, 'ode')
		limit = ET.SubElement(ode, 'limit')
		cfm = ET.SubElement(limit, 'cfm')
		cfm.text = '0'
		erp = ET.SubElement(limit, 'erp')
		erp.text = '0.2'
		susp = ET.SubElement(ode, 'suspension')
		cfm = ET.SubElement(susp, 'cfm')
		cfm.text = '0'
		erp = ET.SubElement(susp, 'erp')
		erp.text = '0.2'

		return root

class MecanumLink:
	def __init__(self, pose, num):
		self.pose = pose
		self.num = num

		## Inertia Parameters
		self.mass = 0.00113845
		self.ixx = 2.53776e-08
		self.ixy = 0
		self.ixz = 0
		self.iyy = 2.53776e-08
		self.iyz = 0
		self.izz = 1.47666e-08

		## Friction Parameters
		self.mu = 1
		self.mu2 = 1
		self.fdir1 = '0 0 0'
		self.slip1 = 1000
		self.slip2 = 0
		self.tor_coeff = 1
		self.tor_slip = 0
		self.bounce_coeff = 0
		self.bounce_thresh = 1e+06
		self.soft_cfm = 0.5
		self.soft_erp = 0.8
		self.kp = 1e+13
		self.kd = 1

	def construct(self):
		root = ET.Element('link', attrib={'name':f'mw-l{self.num}'})
		
		pose = ET.SubElement(root, 'pose')
		pose.text = self.pose

		inertial = ET.SubElement(root, 'inertial')
		mass = ET.SubElement(inertial, 'mass')
		mass.text = str(self.mass)
		inertia = ET.SubElement(inertial, 'inertia')

		ixx = ET.SubElement(inertia, 'ixx')
		ixx.text = str(self.ixx)
		ixy = ET.SubElement(inertia, 'ixy')
		ixy.text = str(self.ixy)
		ixz = ET.SubElement(inertia, 'ixz')
		ixz.text = str(self.ixz)
		iyy = ET.SubElement(inertia, 'iyy')
		iyy.text = str(self.iyy)
		iyz = ET.SubElement(inertia, 'iyz')
		iyz.text = str(self.iyz)
		izz = ET.SubElement(inertia, 'izz')
		izz.text = str(self.izz)

		ipose = ET.SubElement(inertial, 'pose', attrib={'frame':''})
		ipose.text = '0 0 0 0 0 0'

		self_collide = ET.SubElement(root, 'self_collide')
		self_collide.text = '0'

		kinematic = ET.SubElement(root, 'kinematic')
		kinematic.text = '0'

		visual = ET.SubElement(root, 'visual', attrib={'name':f'mw-l{self.num}-v'})
		geom = ET.SubElement(visual, 'geometry')
		mesh = ET.SubElement(geom, 'mesh')
		uri = ET.SubElement(mesh, 'uri')
		uri.text = 'file://mecanum-roller.stl'
		scale = ET.SubElement(mesh, 'scale')
		scale.text = '0.001 0.001 0.001'

		collision = ET.SubElement(root, 'collision', attrib={'name':f'mw-l{self.num}-c'})
		geom = ET.SubElement(collision, 'geometry')
		mesh = ET.SubElement(geom, 'mesh')
		uri = ET.SubElement(mesh, 'uri')
		uri.text = 'file://mecanum-roller.stl'
		scale = ET.SubElement(mesh, 'scale')
		scale.text = '0.001 0.001 0.001'

		surface = ET.SubElement(collision, 'surface')
		friction = ET.SubElement(surface, 'friction')
		ode = ET.SubElement(friction, 'ode')
		mu = ET.SubElement(ode, 'mu')
		mu.text = str(self.mu)
		mu2 = ET.SubElement(ode, 'mu2')
		mu2.text = str(self.mu2)
		fdir1 = ET.SubElement(ode, 'fdir1')
		fdir1.text = str(self.fdir1)
		slip1 = ET.SubElement(ode, 'slip1')
		slip1.text = str(self.slip1)
		slip2 = ET.SubElement(ode, 'slip2')
		slip2.text = str(self.slip2)

		torsional = ET.SubElement(friction, 'torsional')
		coeff = ET.SubElement(torsional, 'coefficient')
		coeff.text = str(self.tor_coeff)
		patch_rad = ET.SubElement(torsional, 'patch_radius')
		patch_rad.text = '0'
		surface_rad = ET.SubElement(torsional, 'surface_radius')
		surface_rad.text = '0'
		use_patch_rad = ET.SubElement(torsional, 'use_patch_radius')
		use_patch_rad.text = '1'

		ode = ET.SubElement(torsional, 'ode')
		slip = ET.SubElement(ode, 'slip')
		slip.text = str(self.tor_slip)

		bounce = ET.SubElement(surface, 'bounce')
		rest_coeff = ET.SubElement(bounce, 'restitution_coefficient')
		rest_coeff.text = str(self.bounce_coeff)

		threshold = ET.SubElement(bounce, 'threshold')
		threshold.text = str(self.bounce_thresh)

		contact = ET.SubElement(surface,'contact')
		collide = ET.SubElement(contact, 'collide_without_contact')
		collide.text = '0'
		collide_without_contact_bit = ET.SubElement(contact, 'collide_without_contact_bitmask')
		collide_without_contact_bit.text = '1'
		collide_bit = ET.SubElement(contact, 'collide_bitmask')
		collide_bit.text = '1'

		ode = ET.SubElement(contact, 'ode')
		soft_cfm = ET.SubElement(ode, 'soft_cfm')
		soft_cfm.text = str(self.soft_cfm)
		soft_erp = ET.SubElement(ode, 'soft_erp')
		soft_erp.text = str(self.soft_erp)
		kp = ET.SubElement(ode, 'kp')
		kp.text = f'{self.kp:.0e}'
		kd = ET.SubElement(ode, 'kd')
		kd.text = str(self.kd)
		max_vel = ET.SubElement(ode, 'max_vel')
		max_vel.text = '0.01'
		min_depth = ET.SubElement(ode, 'min_depth')
		min_depth.text = '0'
	
		bullet = ET.SubElement(contact, 'bullet')
		split_impulse = ET.SubElement(bullet, 'split_impulse')
		split_impulse.text = '1'
		split_impulse_pen = ET.SubElement(bullet, 'split_impulse_penetration_threshold')
		split_impulse_pen.text = '-0.01'
		
		soft_cfm = ET.SubElement(bullet, 'soft_cfm')
		soft_cfm.text = str(self.soft_cfm)
		soft_erp = ET.SubElement(bullet, 'soft_erp')
		soft_erp.text = str(self.soft_erp)
		kp = ET.SubElement(bullet, 'kp')
		kp.text = f'{self.kp:.0e}'
		kd = ET.SubElement(bullet, 'kd')
		kd.text = str(self.kd)


		##NOTE: add contact params

		return root


for g in [1,-1]:
	root = ET.Element('sdf', attrib={'version': '1.11'})
	model = ET.SubElement(root, 'model', attrib={'name': 'mecanum-wheel'})
	static = ET.SubElement(model, 'static')
	static.text = 'false'

	mecanumbase = ET.SubElement(model, 'link', attrib={'name':'mecanum-base'})
	mb_pose = ET.SubElement(mecanumbase, 'pose')
	mb_pose.text = '0 0 0 0 0 0'

	mb_vis = ET.SubElement(mecanumbase, 'visual', attrib={'name':'mb-v'})
	mb_geom = ET.SubElement(mb_vis, 'geometry')
	mb_cyl = ET.SubElement(mb_geom, 'cylinder')
	mb_rad = ET.SubElement(mb_cyl, 'radius')
	mb_rad.text = '0.015'
	mb_length = ET.SubElement(mb_cyl, 'length')
	mb_length.text = '0.01'

	mb_col = ET.SubElement(mecanumbase, 'collision', attrib={'name':'mb-c'})
	mb_col.append(mb_geom)

	## Create Links
	for i in range(0,8):
		## Calculate Pose Translation
		radius = 0.03
		link_x = radius*np.sin(i*np.pi/4)
		link_y = 0
		link_z = radius*np.cos(i*np.pi/4)

		print("Model {}: x={}, y={}, z={}".format(i, link_x, link_y, link_z))

		## Calculate Pose Angles
		# These angles should switch between +-pi/8 for mecanum and mecanum mirror

		# Rotation about z-axis to turn each mecanum link
		Rz = np.array([[np.cos(g*np.pi/4), -np.sin(g*np.pi/4), 0],\
				[np.sin(g*np.pi/4), np.cos(g*np.pi/4), 0],\
				[0, 0, 1]])

		# Rotation about y-axis for each of the links
		Ry = np.array([[np.cos(i*np.pi/4), 0, np.sin(i*np.pi/4)],\
				[0, 1, 0],\
				[-np.sin(i*np.pi/4), 0, np.cos(i*np.pi/4)]])

		##Extrinsic Rotation
		rotation = np.matmul(Ry, Rz)

		rpy_x = np.arctan2(rotation[2][1],rotation[2][2])
		rpy_y = np.arctan2(-rotation[2][0],np.sqrt(rotation[2][1]**2+rotation[2][2]**2))
		rpy_z = np.arctan2(rotation[1][0], rotation[0][0])

		## Create Model
		model.append(MecanumLink(f'{link_x} {link_y} {link_z} {rpy_x} {rpy_y} {rpy_z}',i+1).construct())

	## Create Joints
	for i in range(0,8):
		model.append(MecanumJoint(i+1).construct())

	ET.indent(root, space=' ')
	print(ET.tostring(root, encoding='unicode'))

	if g==1:
		outname = 'gen_mecanum.sdf'
	else:
		outname = 'gen_mecanum_mirror.sdf'

	with open(f'../models/{outname}','w') as wfile:
		wfile.write(ET.tostring(root, encoding='unicode'))




