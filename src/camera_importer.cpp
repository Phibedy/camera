#include <camera_importer.h>
#include <cstdint>
#include <string>
#include <lms/imaging/static_image.h>
#include "lms/imaging/converter.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <lms/config.h>
#include <string.h>
//TODO: Use MMAPING!

bool CameraImporter::initialize() {
    logger.info() << "Init: CameraImporter";

    file = config().get<std::string>("device","");
    int width = config().get<int>("width",0);
    int height = config().get<int>("height",0);
    lms::imaging::Format format =
            lms::imaging::formatFromString(config().get<std::string>("format",
            lms::imaging::formatToString(lms::imaging::Format::YUYV)));
    framerate = config().get<int>("framerate",0);

    if(format == lms::imaging::Format::UNKNOWN) {
        logger.error("init") << "Format is " << format;
        return false;
    }

    // get write permission for data channel
    cameraImagePtr = writeChannel<lms::imaging::Image>("IMAGE");
    cameraImagePtr->resize(width, height, format);

    // init wrapper
    wrapper = new V4L2Wrapper(logger);

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

    wrapper->initBuffersIfNecessary();

    logger.info("camera was set up!");
    
    // Set camera settings

    wrapper->queryCameraControls();
    wrapper->setCameraSettings(&config());
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
        logger.error("Camera Importer: Camera handle not valid!\n");
        while(!valid) {
            wrapper->closeDevice();
            // TODO set format and FPS
            wrapper->openDevice(file);
            valid = wrapper->isValidCamera();

            usleep(100);
        }
        // Set camera settings

        wrapper->queryCameraControls();
        wrapper->setCameraSettings(&config());
        wrapper->queryCameraControls(); // Re-read current controls
        wrapper->printCameraControls();
        return false;
    }

    logger.time("read");
    if(! wrapper->captureImage(*cameraImagePtr)) {
        logger.error("cycle") << "Could not read a full image";
    }
    logger.timeEnd("read");
	return true;
}
