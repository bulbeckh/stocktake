cmake_minimum_required(VERSION 3.22)

find_package(gz-cmake3 REQUIRED)
find_package(sdformat14 REQUIRED)

gz_find_package(gz-sim8 REQUIRED)

set(GZ_PLUGIN_VER ${gz-plugin2_VERSION_MAJOR})
set(GZ_COMMON_VER ${gz-common5_VERSION_MAJOR})

add_library(StocktakeRobot SHARED src/robot.cc)
set_property(TARGET StocktakeRobot PROPERTY CXX_STANDARD 17)

#add_library(StoreGen SHARED src/tankcontroller.cc)
#set_property(TARGET StoreGen PROPERTY CXX_STANDARD 17)

set(include_dirs
	include/
	${gz-sim8_INCLUDE_DIRS}
	${gz-common5_INCLUDE_DIRS}
	${gz-plugin2_INCLUDE_DIRS}
	${gz-msgs10_INCLUDE_DIRS}
	${gz-math7_INCLUDE_DIRS}
	${gz-transport13_INCLUDE_DIRS}
	${sdformat14_INCLUDE_DIRS}
)

target_include_directories(StocktakeRobot PUBLIC ${include_dirs})
#target_include_directories(TankController PUBLIC ${include_dirs})

target_link_libraries(StocktakeRobot
	gz-common${GZ_COMMON_VER}::gz-common${GZ_COMMON_VER}
	gz-plugin${GZ_PLUGIN_VER}::gz-plugin${GZ_PLUGIN_VER}
)

#target_link_libraries(TankController
	#gz-common${GZ_COMMON_VER}::gz-common${GZ_COMMON_VER}
	#gz-plugin${GZ_PLUGIN_VER}::gz-plugin${GZ_PLUGIN_VER}
#)