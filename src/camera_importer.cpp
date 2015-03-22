#include <camera_importer.h>
#include <cstdint>
#include <string>
#include <lms/imaging/static_image.h>
#include "lms/imaging/converter.h"
#include <lms/datamanager.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <lms/type/module_config.h>
#include <string.h>
//TODO: Use MMAPING!

namespace lms_camera_importer {

bool CameraImporter::initialize() {
    logger.info() << "Init: CameraImporter";

    // read config
    cameraConfig = getConfig();

    file = cameraConfig->get<std::string>("device");

    settings.width = cameraConfig->get<int>("width");
    settings.height = cameraConfig->get<int>("height");
    settings.format =
            lms::imaging::formatFromString(cameraConfig->get<std::string>("format",
            lms::imaging::formatToString(lms::imaging::Format::YUYV)));
    settings.framerate = cameraConfig->get<int>("framerate");

    if(settings.format == lms::imaging::Format::UNKNOWN) {
        logger.error("init") << "Format is " << settings.format;
        return false;
    }

    // get write permission for data channel
    cameraImagePtr = datamanager()->writeChannel<lms::imaging::Image>(this, "CAMERA_IMAGE");
    cameraImagePtr->resize(settings.width, settings.height, settings.format);

    // init wrapper
    wrapper = new V4L2Wrapper();

    logger.debug("init") << "Opening " << file << " ...";
    if(! wrapper->openDevice(file, settings)) {
        logger.error("init") << wrapper->error();
        return false;
    }

//    std::vector<Camera::Settings> resolutions;
//    wrapper->getSupportedResolutions(resolutions);
//    for(const V4L2Wrapper::CameraResolution &res: resolutions) {
//        logger.debug("cam") << res.pixelFormat << " "
//                            << res.width << "x" << res.height << " " << res.framerate << " FPS";
//    }

    logger.info("camera was set up!");
    
    // Set camera settings

//    wrapper->queryCameraControls();
//    wrapper->setCameraSettings(cameraConfig);
//    wrapper->queryCameraControls(); // Re-read current controls
//    wrapper->printCameraControls();

    logger.info() << "After query and set!!";

	return true;
}

bool CameraImporter::deinitialize() {
    logger.info("deinit") << "Deinit: CameraImporter";
	//Stop Camera
    if(! wrapper->closeDevice()) {
        logger.error("deinit") << wrapper->error();
    }
    delete wrapper;

	return true;
}

bool CameraImporter::cycle () {
    if(! wrapper->isDeviceReady()) {
        logger.error("cycle") << wrapper->error();

        while(! wrapper->isDeviceReady()) {
            logger.warn("cycle") << "Try to reopen...";

            if(! wrapper->reopenDevice()) {
                logger.error("cycle") << wrapper->error();
            }

            usleep(100);
        }
        // Set camera settings

        wrapper->queryCameraControls();
        wrapper->setCameraSettings(cameraConfig);
        wrapper->queryCameraControls(); // Re-read current controls
        wrapper->printCameraControls();

    }

    logger.time("read");
    if(! wrapper->captureImage(*cameraImagePtr)) {
        logger.error("cycle") << wrapper->error();
    }
    logger.timeEnd("read");

    std::int64_t sleepy = 1000 * 1000 / settings.framerate;
    logger.debug("cycle") << "Sleepy: " << sleepy;
    //usleep(sleepy);

	return true;
}

}  // namespace lms_camera_importer
