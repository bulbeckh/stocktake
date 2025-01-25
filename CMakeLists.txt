cmake_minimum_required(VERSION 3.22)

project(StocktakeRobot)

set(SR_LIBS
	"gz-cmake3"
	"gz-common5"
	"gz-fuel_tools9"
	"gz-gui8"
	"gz-launch7"
	"gz-math7"
	"gz-msgs10"
	"gz-physics7"
	"gz-plugin2"
	"gz-rendering8"
	"gz-sensors8"
	"gz-sim8"
	"gz-tools2"
	"gz-transport13"
	"gz-utils2"
	"sdformat14"
)

## Update CMAKE_PREFIX_PATH variable
#list(APPEND CMAKE_PREFIX_PATH "/home/henry/Documents/contrib/workspace/install/gz-cmake3/share/cmake/gz-cmake3/")
list(APPEND CMAKE_INSTALL_PREFIX "/home/henry/Documents/contrib/workspace/install/")

find_package(gz-cmake3 REQUIRED)
gz_find_package(gz-sim8 REQUIRED)

message(STATUS "here")

#set(include_dirs "include/")
#foreach(i IN LISTS SR_LIBS)
	#string(REGEX REPLACE "^[^-]*-" "" posthyphen ${i})
	#message(STATUS "${i} ${posthyphen}")
	#list(APPEND include_dirs "/home/henry/Documents/contrib/workspace/install/${i}/include/gz/${posthyphen}")
#endforeach()

list(JOIN include_dirs " " id_string)
message(STATUS "Include Dirs:  ${id_string}")

## Find Packages
#foreach(pkg IN LISTS SR_LIBS)
	#find_package(${pkg})
	#message(STATUS "Finding package ${pkg}")
#endforeach()

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

add_library(StocktakeRobot SHARED src/robot.cc)
set_property(TARGET StocktakeRobot PROPERTY CXX_STANDARD 17)

target_include_directories(StocktakeRobot PUBLIC ${include_dirs})
target_link_libraries(StocktakeRobot
	gz-common5
	gz-plugin2
	gz-transport13
	gz-sim8
)

add_library(RFIDTagManager SHARED src/rfidtagmanager.cc src/rfidtag.cc)
target_include_directories(RFIDTagManager PUBLIC ${include_dirs})
target_link_libraries(RFIDTagManager
	gz-common5
	gz-msgs10
	gz-math7
	gz-transport13
	sdformat14
)






