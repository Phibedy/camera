set ( SOURCES 
	"src/camera_importer.cpp"
	"src/interface.cpp"
)

set (HEADERS 
	"include/camera_importer.h"
	${HEADERS_SHARED}
)

include_directories("include")

add_library ( camera MODULE ${SOURCES} ${HEADERS})
target_link_libraries(camera PRIVATE lms)
