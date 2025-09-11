cmake_minimum_required(VERSION 3.8)
project(odometry_fus)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(PCL REQUIRED)
find_package(Eigen3 REQUIRED)

include_directories(
  ${PCL_INCLUDE_DIRS}
  ${Eigen3_INCLUDE_DIRS}
)

add_definitions(${PCL_DEFINITIONS})

# Узел fusion odometry
add_executable(odometry_fus_node
  src/odometry_fus.cpp
)

ament_target_dependencies(odometry_fus_node
  rclcpp
  nav_msgs
  sensor_msgs
  geometry_msgs
  tf2_ros
  tf2_geometry_msgs
  std_msgs
)

add_executable(icp_odometer src/icp_odometer.cpp)

ament_target_dependencies(icp_odometer
  rclcpp
  sensor_msgs
  nav_msgs
  geometry_msgs
  tf2_ros
  tf2
)

target_link_libraries(icp_odometer
  ${PCL_LIBRARIES}
)

add_executable(motion_emulator
  src/motion_emulator.cpp
)

ament_target_dependencies(motion_emulator
  rclcpp
  geometry_msgs
  nav_msgs
  tf2
  tf2_ros
  tf2_geometry_msgs
)


target_link_libraries(icp_odometer
  ${PCL_LIBRARIES}
  ${PCL_COMMON_LIBRARIES}
  ${PCL_IO_LIBRARIES}
  ${PCL_FILTERS_LIBRARIES}
  ${PCL_REGISTRATION_LIBRARIES}
)

install(TARGETS 
  odometry_fus_node
  motion_emulator
  icp_odometer
  DESTINATION lib/${PROJECT_NAME}
)

ament_export_dependencies(
  rclcpp
  sensor_msgs
  nav_msgs
  geometry_msgs
  tf2_ros
  tf2
  PCL
  Eigen3
)

ament_package()