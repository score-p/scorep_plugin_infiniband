cmake_minimum_required(VERSION 3.12)

project(infiniband_plugin)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(PLUGIN_SOURCE src/infiniband_plugin.cpp)

add_subdirectory(lib/scorep_plugin_cxx_wrapper)

add_library(infiniband_plugin SHARED ${PLUGIN_SOURCE})
include_directories(infiniband_plugin PUBLIC
	include
)
target_link_libraries(infiniband_plugin PUBLIC Scorep::scorep-plugin-cxx)

# add_executable(get_supported_sensors src/scorep_infiniband_dump_ports.cpp)
# target_link_libraries(get_supported_sensors PRIVATE infiniband_plugin)

install(TARGETS infiniband_plugin LIBRARY DESTINATION lib)
