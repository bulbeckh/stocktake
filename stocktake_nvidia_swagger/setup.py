from setuptools import find_packages, setup

package_name = 'stocktake_nvidia_swagger'

setup(
    name=package_name,
    version='0.0.1',
    packages=find_packages(),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Henry Bulbeck',
    maintainer_email='henrybulbeck@gmail.com',
    description='ROS2 Node interface to NVIDIA SWAGGER waypoint generation',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'server_node = stocktake_nvidia_swagger.swagger_graph_server_node:main',
            'test_client = stocktake_nvidia_swagger.test_client:main',
        ],
    },
)
