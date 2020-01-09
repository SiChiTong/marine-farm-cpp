cmake_minimum_required(VERSION 2.8.3)
project(mf_navigation)

## Compile as C++11, supported in ROS Kinetic and newer
add_compile_options(-std=c++11)

# set(CMAKE_CXX_FLAGS_DEBUG "-g -fno-omit-frame-pointer")
# set(ROS_BUILD_TYPE Debug)

set(EIGEN_BUILD_FLAGS "-O3 -DEIGEN_NO_DEBUG")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${EIGEN_BUILD_FLAGS}")
set(ROS_BUILD_TYPE Release)


########################
##  Configure Catkin  ##
########################

find_package(catkin REQUIRED COMPONENTS
  nodelet
  roscpp
  tf2
  tf2_ros
  geometry_msgs
  visualization_msgs
  mf_robot_model
  mf_sensors_simulator
  mf_mapping
)

catkin_package(
  INCLUDE_DIRS include
  CATKIN_DEPENDS nodelet roscpp
)


###########
## Build ##
###########

include_directories(
  include/mf_navigation
  ${catkin_INCLUDE_DIRS}
)

find_package(Eigen3 REQUIRED)
include_directories(${Eigen_INCLUDE_DIRS})

# Planning nodelet
add_library(planning_nodelet
  src/mf_navigation/planning_nodelet.cpp
  src/mf_navigation/planning.cpp
)
add_dependencies(planning_nodelet
  ${catkin_EXPORTED_TARGETS}
  mf_sensors_simulator_generate_messages_cpp
  mf_mapping_generate_messages_cpp
)
target_link_libraries(planning_nodelet ${catkin_LIBRARIES})