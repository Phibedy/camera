#ifndef IMPORTER_IMAGE_IMPORTER_H
#define IMPORTER_IMAGE_IMPORTER_H

#include <stdio.h>

#include <vector>

#include <linux/videodev2.h>

#include <lms/datamanager.h>
#include <lms/module.h>
#include <lms/type/static_image.h>
#include <lms/type/module_config.h>

class CameraImporter : public lms::Module {
public:

    bool initialize();
    bool deinitialize();

    bool cycle();

protected:

    lms::type::StaticImage<320,240,char> imageData;
    const lms::type::ModuleConfig* cameraConfig;
    std::string file;
    int width;
    int height;
    int bpp;
    int framerate;

    uint8_t* camera_buffer;
    int bufsize;

    int fd_camera;

	// Camera Controls
	std::string cameraSettingsFile;
    std::map<std::string, struct v4l2_queryctrl> cameraControls;


    bool checkCameraFileHandle();

    //TODO User controls
    bool setCameraSettings();
    bool queryCameraControls();
    bool printCameraControls();
    template<typename T> T getControl(uint32_t id);
    template<typename T> T getControl(const std::string& name);
    template<typename T> bool setControl(uint32_t id, T value);
    template<typename T> bool setControl(const std::string& name, T value);
};

#endif
