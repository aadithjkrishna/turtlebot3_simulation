cmake_minimum_required(VERSION 3.8)
project(my_nav_nodes)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -std=c++17)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

# Build Target
add_executable(wall_follower src/wall_follower.cpp)
ament_target_dependencies(wall_follower rclcpp geometry_msgs sensor_msgs)

# Install Rules
install(TARGETS
  wall_follower
  DESTINATION lib/${PROJECT_NAME}
)

export(PACKAGE ${PROJECT_NAME})
ament_package()