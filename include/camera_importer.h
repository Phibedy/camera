#ifndef IMPORTER_IMAGE_IMPORTER_H
#define IMPORTER_IMAGE_IMPORTER_H

#include <stdio.h>
#include <cstdint>

#include <vector>

#include <linux/videodev2.h>

#include <lms/datamanager.h>
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

    const lms::type::ModuleConfig* cameraConfig;
    std::string file;
    int framerate;

    lms::imaging::Image rawImage;
    lms::imaging::Image *grayImagePtr;

    V4L2Wrapper *wrapper;

    // TODO move this into its own module
    // ONLY FOR TESTING
    void save_ppm(const std::string &filename, const lms::imaging::Image &image);
};

}  // namespace lms_camera_importer

#endif
