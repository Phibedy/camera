#ifndef IMPORTER_IMAGE_IMPORTER_H
#define IMPORTER_IMAGE_IMPORTER_H

#include <stdio.h>
#include <cstdint>

#include <vector>

#include <linux/videodev2.h>

#include <lms/module.h>
#include <lms/type/module_config.h>
#include <lms/imaging/image.h>
#include "v4l2_wrapper.h"

namespace lms_camera_importer {

class CameraImporter : public lms::Module {
public:

    bool initialize();
    bool deinitialize();

    bool cycle();

protected:
    std::string file;
    int framerate;

    lms::WriteDataChannel<lms::imaging::Image> cameraImagePtr;

    V4L2Wrapper *wrapper;
};

}  // namespace lms_camera_importer

#endif
