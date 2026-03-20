cmake_minimum_required(VERSION 3.8)
project(stocktake_frontend)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

install(DIRECTORY frontend
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
