#ifndef LMS_CAMERA_IMPORTER_CAMERA_H
#define LMS_CAMERA_IMPORTER_CAMERA_H

#include <cstdint>
#include <string>

#include "lms/imaging/format.h"
#include "lms/imaging/image.h"

namespace lms_camera_importer {

struct CameraDevice {
    std::string device;
    std::string description;
};

class Camera {
public:
    virtual ~Camera() {}

    struct Settings {
        std::string description;
        lms::imaging::Format format;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t framerate;
    };

    /**
     * @brief Return a list of all camera devices that
     * can be captured by the wrapper.
     *
     * @return list of devices
     */
    virtual std::vector<CameraDevice> getAvailableDevices() =0;

    virtual bool openDevice(const std::string &device, const Settings &settings) =0;
    virtual bool closeDevice() =0;
    virtual bool captureImage(lms::imaging::Image &image) =0;
    bool reopenDevice();
    virtual bool isDeviceReady() =0;

    std::uint32_t width() const;
    std::uint32_t height() const;
    lms::imaging::Format format() const;
    std::uint32_t framerate() const;

    /**
     * @brief Get the error message if any after a call to openDevice(), closeDevice(),
     * reopenDevice(), captureImage() or isDeviceReady(). The error message will be empty
     * string if the method the API call returned true.
     *
     * @return error message
     */
    std::string error() const;

    /**
     * @brief Returns all camera settings that are valid to set
     * while opening the device. The call to this method is only
     * valid after opening a device.
     */
    virtual const std::vector<Settings>& getValidCameraSettings() =0;
protected:
    std::string m_device;
    Settings m_settings;

    /**
     * @brief Set the error message.
     *
     * This method should be used in Camera implementations
     * to show that an error occured.
     *
     * Use it like this:
     * if(someApiCall() == ERROR_CODE) {
     *   return error("ERROR_CODE was triggered after call to someApiCall");
     * }
     *
     * @param message error message to set
     * @return always false
     */
    bool error(const std::string &message);

    /**
     * @brief Empty the error message
     *
     * This method should be used in Camera implementations
     * to show that no error occured.
     *
     * Call it at the end of a method to remove any
     * previous error messages.
     *
     * @return always true
     */
    bool success();
private:
    std::string m_errorMessage;
};

}  // namespace lms_camera_importer

#endif /* LMS_CAMERA_IMPORTER_CAMERA_H */
