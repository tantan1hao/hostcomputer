cmake_minimum_required(VERSION 3.0.2)
project(ros1_bridge)

find_package(catkin REQUIRED COMPONENTS
  rospy
  geometry_msgs
  std_msgs
  std_srvs
)

catkin_package()

catkin_install_python(PROGRAMS
  host_bridge_node.py
  mediamtx_launcher.sh
  video_manager_node.py
  DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY launch
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)
