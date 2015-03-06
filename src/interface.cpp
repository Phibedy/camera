#include <camera_importer.h>

extern "C" {
void* getInstance () {
	return new CameraImporter();
}
}
