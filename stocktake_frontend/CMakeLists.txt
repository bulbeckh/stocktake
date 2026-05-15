cmake_minimum_required(VERSION 3.8)
project(stocktake_frontend)

find_package(ament_cmake REQUIRED)

set(FRONTEND_DIR ${CMAKE_CURRENT_SOURCE_DIR}/frontend)

add_custom_target(npm_install ALL
  COMMAND npm ci --loglevel=error --no-progress --no-audit --no-fund
  WORKING_DIRECTORY ${FRONTEND_DIR}
  COMMENT "Installing Next.js dependencies"
)

add_custom_target(next_build ALL
  COMMAND npm run build --silent
  WORKING_DIRECTORY ${FRONTEND_DIR}
  DEPENDS npm_install
  COMMENT "Building Next.js app"
)

install(DIRECTORY ${FRONTEND_DIR}/.next ${FRONTEND_DIR}/node_modules
  DESTINATION share/${PROJECT_NAME}/frontend
)

install(FILES ${FRONTEND_DIR}/package.json
  DESTINATION share/${PROJECT_NAME}/frontend
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
