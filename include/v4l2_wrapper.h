#ifndef LMS_CAMERA_IMPORTER_V4L2_WRAPPER
#define LMS_CAMERA_IMPORTER_V4L2_WRAPPER

#include <linux/videodev2.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>

#include <memory>
#include <cstdint>
#include <string>
#include <map>
#include <vector>

#include "lms/type/module_config.h"
#include "lms/imaging/format.h"
#include "lms/imaging/image.h"
#include "lms/logger.h"

namespace lms_camera_importer {

int xioctl(int64_t fh, int64_t request, void *arg);

class V4L2Wrapper {
 public:
    struct CameraResolution {
        std::string pixelFormat;
        std::uint32_t internalPixelFormat;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t framerate;
    };

    V4L2Wrapper(lms::logging::Logger *rootLogger);

    /**
     * @brief Open a device path, e.g. "/dev/video0"
     * @param devicePath usually something inside /dev/
     * @return true if opening was successful, otherwise false
     */
    bool openDevice(const std::string &devicePath);

    /**
     * @brief Close the camera file desciptor.
     *
     * If the file descriptor was already closed or never opened
     * the nothing happens.
     *
     * @return true if closing was successful, otherwise false
     */
    bool closeDevice();

    /**
     * @brief Check if the device was opened an not yet closed.
     * @return true if open, otherwise false
     */
    bool isOpen();

    /**
     * @brief Set width, height and pixel format of the captured images.
     * @param width width of a frame
     * @param height height of a frame
     * @param fmt pixel format, e.g. V4L2_PIX_FMT_YUYV
     * @return true if the operation was successful, otherwise false
     */
    bool setFormat(std::uint32_t width, std::uint32_t height, lms::imaging::Format fmt);

    /**
     * @brief Set the framerate of the camera
     * @param framerate examples are 60 or 100
     * @return true if successful, otherwise false
     */
    bool setFramerate(std::uint32_t framerate);

    /**
     * @brief Get the framerate of the camera
     * @return framerate, or 0 if failed
     */
    std::uint32_t getFramerate();

    /**
     * @brief Check if the device is a video capturing device.
     * @return true if camera is valid for capturing
     */
    bool isValidCamera();

    bool setCameraSettings(const lms::type::ModuleConfig *cameraConfig);
    bool queryCameraControls();
    bool printCameraControls();

    void getSupportedResolutions(std::vector<CameraResolution> &result);

    bool captureImage(lms::imaging::Image &image);

 private:
    lms::logging::ChildLogger logger;
    std::string devicePath;
    int fd;

    std::map<std::string, struct v4l2_queryctrl> cameraControls;

    static std::uint32_t toV4L2(lms::imaging::Format fmt);

    std::int32_t getControl(std::uint32_t id);
    std::int32_t getControl(const std::string& name);
    bool setControl(std::uint32_t id, std::int32_t value);
    bool setControl(const std::string& name, std::int32_t value);

    void getSupportedFramesizes(std::vector<CameraResolution> &result, CameraResolution res);
    void getSupportedFramerates(std::vector<CameraResolution> &result,
                                CameraResolution res);
};

}  // namespace lms_camera_importer

#endif /* LMS_CAMERA_IMPORTER_V4L2_WRAPPER */
