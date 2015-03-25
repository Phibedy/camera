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
#include "camera.h"

namespace lms_camera_importer {

int xioctl(int64_t fh, int64_t request, void *arg);

class V4L2Wrapper : public Camera {
 public:
    /**
     * @brief Open a device path, e.g. "/dev/video0"
     * @param devicePath usually something inside /dev/
     * @return OK if successful
     */
    bool openDevice(const std::string &device, const Settings &settings) override;

    /**
     * @brief Close the camera file desciptor.
     *
     * If the file descriptor was already closed or never opened
     * the nothing happens.
     *
     * @return true if closing was successful, otherwise false
     */
    bool closeDevice() override;

    /**
     * @brief Check if the device was opened and to closed
     * and is ready to capture images.
     *
     * @return true if device is ready, false otherwise
     */
    bool isDeviceReady() override;

    bool setCameraSettings(const lms::type::ModuleConfig *cameraConfig);
    bool queryCameraControls();
    bool printCameraControls();

    const std::vector<Settings>& getValidCameraSettings();

    bool captureImage(lms::imaging::Image &image) override;

 private:
    bool perror(const std::string &msg);

    int fd;

    /**
     * @brief Should be either V4L2_CAP_READWRITE or V4L2_CAP_STREAMING.
     */
    std::uint32_t ioType;

    std::map<std::string, struct v4l2_queryctrl> cameraControls;

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

    static std::uint32_t toV4L2(lms::imaging::Format fmt);

    static lms::imaging::Format fromV4L2(std::uint32_t fmt);

    std::int32_t getControl(std::uint32_t id);
    std::int32_t getControl(const std::string& name);
    bool setControl(std::uint32_t id, std::int32_t value);
    bool setControl(const std::string& name, std::int32_t value);

    std::vector<Settings> cachedValidSettings;

    void getSupportedResolutions(std::vector<Settings> &result);
    void getSupportedFramesizes(std::vector<Settings> &result,
                                const std::string &description, std::uint32_t pixelFormat);
    void getSupportedFramerates(std::vector<Settings> &result,
                                const std::string &description, std::uint32_t pixelFormat,
                                std::uint32_t width, std::uint32_t height);

    // for MMAPPING:
    struct MapBuffer {
        void *start;
        size_t length;
    };

    bool initBuffers();
    bool queueBuffers();
    bool destroyBuffers();
    MapBuffer *buffers;
    unsigned int numBuffers;
};

}  // namespace lms_camera_importer

#endif /* LMS_CAMERA_IMPORTER_V4L2_WRAPPER */
