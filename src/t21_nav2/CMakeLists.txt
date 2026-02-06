cmake_minimum_required(VERSION 3.8)
project(t21_nav2)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(pluginlib REQUIRED)
find_package(behaviortree_cpp_v3 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(nav2_behavior_tree REQUIRED)


add_library(spin_towards_trajectory_plugin SHARED src/spin_towards_trajectory.cpp)

pluginlib_export_plugin_description_file(nav2_behavior_tree plugins.xml)

target_include_directories(spin_towards_trajectory_plugin PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(spin_towards_trajectory_plugin PRIVATE "BT_PLUGIN_EXPORT")

ament_target_dependencies(spin_towards_trajectory_plugin
    behaviortree_cpp_v3
    rclcpp
    tf2_geometry_msgs
    nav2_behavior_tree
)

ament_export_libraries(spin_towards_trajectory_plugin)
ament_export_dependencies(behaviortree_cpp_v3 rclcpp tf2_geometry_msgs nav2_behavior_tree)

add_executable(speed_constraint src/speed_constraint.cpp)

ament_target_dependencies(speed_constraint rclcpp geometry_msgs)

install(TARGETS
  speed_constraint
  DESTINATION lib/${PROJECT_NAME})

install(TARGETS spin_towards_trajectory_plugin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY 
  launch 
  config 
  include
  BT
  DESTINATION share/${PROJECT_NAME}
)

ament_package()