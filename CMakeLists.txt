set ( SOURCES 
	"src/camera_importer.cpp"
	"src/interface.cpp"
)

set (HEADERS 
	"include/camera_importer.h"
	${HEADERS_SHARED}
)

include_directories("include")

add_library ( camera_importer MODULE ${SOURCES} ${HEADERS})
target_link_libraries(camera_importer PRIVATE lms)
