cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(EXTRA_COMPONENT_DIRS
	"${CMAKE_SOURCE_DIR}/app"
	"${CMAKE_SOURCE_DIR}/api"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(glytt)