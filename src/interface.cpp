#include <camera_importer.h>

extern "C" {
void* getInstance () {
    return new lms_camera_importer::CameraImporter();
}
}
