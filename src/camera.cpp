#include "camera.h"

namespace lms_camera_importer {

std::uint32_t Camera::width() const {
    return m_settings.width;
}

std::uint32_t Camera::height() const {
    return m_settings.height;
}

lms::imaging::Format Camera::format() const {
    return m_settings.format;
}

std::uint32_t Camera::framerate() const {
    return m_settings.framerate;
}

std::string Camera::error() const {
    return m_errorMessage;
}

bool Camera::error(const std::string &message) {
    this->m_errorMessage = message;
    return false;
}

bool Camera::success() {
    this->m_errorMessage = "";
    return true;
}

bool Camera::reopenDevice() {
    closeDevice();  // ignore errors here

    if(! openDevice(m_device, m_settings)) {
        return false;
    }

    return true;
}

}  // namespace lms_camera_importer
