cmake_minimum_required(VERSION 2.8)

project(find-min-required-number-of-timer-queries)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -s")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,-O1")

add_executable(find-min-required-number-of-timer-queries src/find-min-required-number-of-timer-queries.c)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})
target_link_libraries(find-min-required-number-of-timer-queries ${OPENGL_LIBRARIES} ${GLUT_LIBRARY})
