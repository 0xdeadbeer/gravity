cmake_minimum_required(VERSION 3.25)
project(gravity C)

set(SOURCE_FILES main.c)
set(HEADER_FILES )

add_executable(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
# dependencies
add_custom_target(assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/assets
        ${PROJECT_BINARY_DIR}/assets
        COMMENT "Copying assets to build folder")

add_dependencies(gravity assets)

# We need a CMAKE_DIR with some code to find external dependencies
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(GLEW REQUIRED)
find_package(assimp REQUIRED)
find_package(cglm REQUIRED)

include_directories(${PROJECT_NAME} ${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIRS} ${CGLM_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES} ${GLEW_LIBRARIES} ${ASSIMP_LIBRARIES} ${CGLM_LIBRARIES} m)
