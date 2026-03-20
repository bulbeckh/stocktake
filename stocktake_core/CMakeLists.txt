cmake_minimum_required(VERSION 3.8)
project(stocktake_core)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch models worlds
  DESTINATION share/${PROJECT_NAME}
)

ament_environment_hooks(
  "${CMAKE_CURRENT_SOURCE_DIR}/hooks/stocktake_core_pkg.dsv.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/hooks/stocktake_core_pkg.sh.in"
)

ament_package()



