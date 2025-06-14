cmake_minimum_required(VERSION 3.25.1)
project(str_project)

set(CMAKE_CXX_STANDARD 20)

# Encuentra OpenCV
find_package(OpenCV REQUIRED)

# Incluye los directorios de OpenCV
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(str_project src/main.cpp
        src/threads/sensor.cpp
        src/threads/camera.cpp
        src/threads/supervisor.cpp
        src/threads/communicator.cpp
        src/threads/barrera.cpp)

# Enlaza las bibliotecas de OpenCV
target_link_libraries(str_project ${OpenCV_LIBS} pigpio curl)
