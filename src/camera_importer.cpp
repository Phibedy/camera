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
    int width = cameraConfig->get<int>("width");
    int height = cameraConfig->get<int>("height");
    lms::imaging::Format format =
            lms::imaging::formatFromString(cameraConfig->get<std::string>("format",
            lms::imaging::formatToString(lms::imaging::Format::YUYV)));
    framerate = cameraConfig->get<int>("framerate");

    if(format == lms::imaging::Format::UNKNOWN) {
        logger.error("init") << "Format is " << format;
        return false;
    }

    // get write permission for data channel
    cameraImagePtr = datamanager()->writeChannel<lms::imaging::Image>(this, "CAMERA_IMAGE");
    cameraImagePtr->resize(width, height, format);

    // init wrapper
    wrapper = new V4L2Wrapper(&logger);

    logger.debug("init") << "Opening " << file << " ...";
    if(! wrapper->openDevice(file)) {
        return false;
    }

    std::vector<V4L2Wrapper::CameraResolution> resolutions;
    wrapper->getSupportedResolutions(resolutions);
    for(const V4L2Wrapper::CameraResolution &res: resolutions) {
        logger.debug("cam") << res.pixelFormat << " "
                            << res.width << "x" << res.height << " " << res.framerate << " FPS";
    }

    logger.debug("init") << "Setting format " << width << "x" << height << " ...";
    if(! wrapper->setFormat(width, height, format)) {
        return false;
    }

    logger.debug("init") << "Setting FPS " << framerate << " ...";
    if(! wrapper->setFramerate(framerate)) {
        return false;
    }

    logger.debug("init") << "Try getFramerate";
    logger.debug("init") << "FPS: " << wrapper->getFramerate();

    logger.debug("init") << "Checking device ...";
    if(! wrapper->isValidCamera()) {
        return false;
    }

    logger.info("camera was set up!");
    
    // Set camera settings

    wrapper->queryCameraControls();
    wrapper->setCameraSettings(cameraConfig);
    wrapper->queryCameraControls(); // Re-read current controls
    wrapper->printCameraControls();

    logger.info() << "After query and set!!";

	return true;
}

bool CameraImporter::deinitialize() {
    logger.info("deinit") << "Deinit: CameraImporter";
	//Stop Camera
    wrapper->closeDevice();
    delete wrapper;

	return true;
}

bool CameraImporter::cycle () {
    if (! wrapper->isOpen()) {
        logger.error("cycle") << "fd_camera is NULL";
        return false;
    }

    //Read Camera
    bool valid = wrapper->isValidCamera();

    //TODO Nicht so geil
    if(!valid) {
        printf("Camera Importer: Camera handle not valid!\n");
        while(!valid) {
            wrapper->closeDevice();
            // TODO set format and FPS
            wrapper->openDevice(file);
            valid = wrapper->isValidCamera();

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
        logger.error("cycle") << "Could not read a full image";
    }
    logger.timeEnd("read");

    std::int64_t sleepy = 1000 * 1000 / framerate;
    logger.debug("cycle") << "Sleepy: " << sleepy;
    usleep(sleepy);

	return true;
}

}  // namespace lms_camera_importer
