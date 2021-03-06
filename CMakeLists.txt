

cmake_minimum_required (VERSION 2.6)
project(FRC_aling)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

add_definitions( -Wno-deprecated )

find_package(Boost COMPONENTS  program_options system filesystem REQUIRED)

# set our library and executable destination dirs
set( EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin" )

# define compiler flags for all code
set( CMAKE_BUILD_TYPE Release )

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories("${PROJECT_SOURCE_DIR}/lib/")
include_directories("${PROJECT_SOURCE_DIR}/lib/bamtools/src")
include_directories( ${Boost_INCLUDE_DIRS} )
include_directories("${PROJECT_SOURCE_DIR}/lib/samtools")
include_directories("${PROJECT_SOURCE_DIR}/lib/htslib")

# sources to compile
file(GLOB FRC_FILES
    ${PROJECT_SOURCE_DIR}/src/FRC_align.cpp
    ${PROJECT_SOURCE_DIR}/src/data_structures/Contig.cpp
    ${PROJECT_SOURCE_DIR}/src/data_structures/Features.cpp
    ${PROJECT_SOURCE_DIR}/src/data_structures/FRC.cpp
    ${PROJECT_SOURCE_DIR}/src/data_structures/qaTools.cpp
)


add_subdirectory(lib)

# FRC executable
add_executable(FRC ${FRC_FILES})
target_link_libraries(FRC ${ZLIB_LIBRARIES})
target_link_libraries(FRC  BamTools)
target_link_libraries(FRC ${Boost_LIBRARIES})
target_link_libraries(FRC  SamTools)
target_link_libraries(FRC  HTSLib)
