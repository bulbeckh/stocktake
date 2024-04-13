#!/usr/bin/python3

'''
Python script to determine the correct angle of the mecanum wheel

Should be configurable for an arbitrary number of mecanum rollers

See mecanum wheel sketch for more detail
'''

import numpy as np

if __name__=="__main__":
	for i in range(0,8):
		## Units in metres
		radius=0.03

print('x1: {}, x2: {}'.format(0.03*np.cos(np.pi/4), 0.03*np.sin(np.pi/4)))


## REDO for 

for i in range(0,8):
	## These angles should switch between +-pi/8 for mecanum and mecanum mirror

	## Rotation about z-axis to turn each mecanum link
	Rz = np.array([[np.cos(-1*np.pi/4), -np.sin(-1*np.pi/4), 0],\
				[np.sin(-1*np.pi/4), np.cos(-1*np.pi/4), 0],\
				[0, 0, 1]])

	## Rotation about y-axis for each of the links
	Ry = np.array([[np.cos(i*np.pi/4), 0, np.sin(i*np.pi/4)],\
				[0, 1, 0],\
				[-np.sin(i*np.pi/4), 0, np.cos(i*np.pi/4)]])

	rotation = np.matmul(Ry, Rz)

	rpy_x = np.arctan2(rotation[2][1],rotation[2][2])
	rpy_y = np.arctan2(-rotation[2][0],np.sqrt(rotation[2][1]**2+rotation[2][2]**2))
	rpy_z = np.arctan2(rotation[1][0], rotation[0][0])

	print(f'link {i}: {rpy_x} {rpy_y} {rpy_z}')

