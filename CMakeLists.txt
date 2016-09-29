cmake_minimum_required(VERSION 2.8.12)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set ( SOURCES
	"src/camera_importer.cpp"
	"src/v4l2_wrapper.cpp"
	"src/interface.cpp"
)

set (HEADERS
	"include/camera_importer.h"
        "include/v4l2_wrapper.h"
)

include_directories("include")

#set compiler flags
if((${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang") OR (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU"))
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wreturn-type -Wpedantic ")
endif()

if(UNIX)
    add_library (camera_importer MODULE ${SOURCES} ${HEADERS})
    target_link_libraries(camera_importer PRIVATE ${CONAN_LIBS})
else(UNIX)
    message(ERROR "only unix support!")
endif()
