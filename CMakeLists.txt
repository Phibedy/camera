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
	${HEADERS_SHARED}
)

include_directories("include")

if(UNIX AND NOT APPLE)
    add_library ( camera_importer MODULE ${SOURCES} ${HEADERS})
    target_link_libraries(camera_importer PRIVATE ${CONAN_LIBS})
endif()
