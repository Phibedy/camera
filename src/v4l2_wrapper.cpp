#include "v4l2_wrapper.h"
#include "sys/mman.h"  // mmap, munmap
#include "lms/extra/time.h"

namespace lms_camera_importer {

int xioctl(int64_t fh, int64_t request, void *arg)
{
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

bool V4L2Wrapper::perror(const std::string &msg) {
    return error(msg + " " + strerror(errno));
}

bool V4L2Wrapper::openDevice(const std::string &device, const Settings &settings) {
    this->m_device = device;
    this->m_settings = settings;

    // open camera device
    fd = ::open(device.c_str(), O_RDWR /* O_RDONLY */);

    if(fd == -1) {
        fd = 0;
        return perror("Could not open device");
    }

    // check for device capabilities
    struct v4l2_capability cap;
    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        return perror("VIDIOC_QUERYCAP");
    }

    // check if device is a camera that can capture images
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        return error("No Video Capture Device");
    }

    // check which capture mode to use
    if(cap.capabilities & V4L2_CAP_STREAMING) {
        ioType = V4L2_CAP_STREAMING;
    } else if (cap.capabilities & V4L2_CAP_READWRITE) {
        ioType = V4L2_CAP_READWRITE;
    } else {
        ioType = 0;
        return error("Device does not provide any supported IO modes");
    }

    if(! setFormat(settings.width, settings.height, settings.format)) {
        return false; // do not use error(...) here, we would overwrite the error message
    }

    if(! setFramerate(settings.framerate)) {
        return false;
    }

    // For memory mapping
    if(ioType == V4L2_CAP_STREAMING) {
        std::cout << "Before init buffers" <<std::endl;
        if(! initBuffers()) {
            return false;
        }

        std::cout << "Before queueBuffers" << std::endl;
        if(! queueBuffers()) {
            return false;
        }
        std::cout << "Buffer ready" << std::endl;
    }

    return success();
}

bool V4L2Wrapper::closeDevice() {
    if(fd != 0) {
        if(ioType == V4L2_CAP_STREAMING && ! destroyBuffers()) {
            fd = 0;
            return false;
        }

        if(close(fd) == -1) {
            fd = 0;
            return perror("close");
        }

        fd = 0;
    }

    return success();
}

bool V4L2Wrapper::isDeviceReady() {
    if(fd == 0) {
        return error("FD is null");
    }

    struct v4l2_capability cap;
    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        return perror("VIDIOC_QUERYCAP");
    }

    return success();
}

std::uint32_t V4L2Wrapper::toV4L2(lms::imaging::Format fmt) {
    using lms::imaging::Format;

    switch(fmt) {
    case Format::GREY: return V4L2_PIX_FMT_GREY;
    case Format::YUYV: return V4L2_PIX_FMT_YUYV;
    case Format::UNKNOWN:  // go to default
    default: return 0;  // This should never happen
    }
}

lms::imaging::Format V4L2Wrapper::fromV4L2(std::uint32_t fmt) {
    using lms::imaging::Format;

    switch(fmt) {
    case V4L2_PIX_FMT_GREY: return Format::GREY;
    case V4L2_PIX_FMT_YUYV: return Format::YUYV;
    default: return Format::UNKNOWN;
    }
}

bool V4L2Wrapper::setFormat(std::uint32_t width, std::uint32_t height, lms::imaging::Format fmt) {
    // http://linuxtv.org/downloads/v4l-dvb-apis/vidioc-g-fmt.html

    // get current pixel format of camera
    v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl (fd, VIDIOC_G_FMT, &format)) {
        return perror("VIDIOC_G_FMT");
    }

    int bytesPerPixel = lms::imaging::bytesPerPixel(fmt);

    if(bytesPerPixel <= 0) {
        return error("bytesPerPixel is invalid");
    }

    // http://linuxtv.org/downloads/v4l-dvb-apis/pixfmt.html#idp22265936
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = toV4L2(fmt);
    format.fmt.pix.bytesperline = width * bytesPerPixel;
    format.fmt.pix.sizeimage = lms::imaging::imageBufferSize(width, height, fmt);

    if(format.fmt.pix.pixelformat == 0) {
        return error("Format is not supported " + lms::imaging::formatToString(fmt));
    }

    // try to set new values
    if (-1 == xioctl (fd, VIDIOC_S_FMT, &format)) {
        return perror("VIDIOC_S_FMT");
    }

    // check if the settings are accepted
    if(format.fmt.pix.width != width || format.fmt.pix.height != height
            || format.fmt.pix.pixelformat != toV4L2(fmt)) {

        return error("Could not set width/height/pixelformat");
    }

    return success();
}

bool V4L2Wrapper::setFramerate(std::uint32_t framerate) {
    // first set the framerate
    v4l2_streamparm streamparm;
    v4l2_fract *tpf;
    memset (&streamparm, 0, sizeof (streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tpf = &streamparm.parm.capture.timeperframe;
    tpf->numerator = 1;
    tpf->denominator = framerate;

    if (xioctl(fd, VIDIOC_S_PARM, &streamparm) == -1) {
        return perror("VIDIOC_S_PARM");
    }

    // and now check the framerate
    memset (&streamparm, 0, sizeof (streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_G_PARM, &streamparm) == -1) {
        return perror("VIDIOC_G_PARM");
    }

    if(! (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        return error("Camera does not support FPS setting");
    }

    tpf = &streamparm.parm.capture.timeperframe;
    std::uint32_t newFramerate = tpf->denominator / tpf->numerator;

    if(newFramerate != framerate) {
        return error("Could not set framerate " + std::to_string(framerate) +
                     ", is now " + std::to_string(newFramerate));
    }

    return success();
}

std::int32_t V4L2Wrapper::getControl(std::uint32_t id)
{
    if( V4L2_CTRL_ID2CLASS(id) == V4L2_CTRL_CLASS_USER )
    {
        // Standard control interface
        struct v4l2_control ctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        ctrl.id = id;
        xioctl(fd, VIDIOC_G_CTRL, &ctrl);
        return ctrl.value;
    }
    else
    {
        // Extended control interface
        struct v4l2_ext_control ctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        struct v4l2_ext_controls ctrls;
        memset(&ctrls, 0, sizeof(ctrls));

        ctrl.id = id;

        ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(ctrl.id);
        ctrls.count = 1;
        ctrls.controls = &ctrl;

        xioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);

        return ctrl.value;
    }
}

std::int32_t V4L2Wrapper::getControl(const std::string& name)
{
    if( cameraControls.find(name) == cameraControls.end() )
    {
        return false;  // TODO WHAT?
    }
    return getControl(cameraControls[name].id);
}

bool V4L2Wrapper::setControl(uint32_t id, std::int32_t value)
{
    if( V4L2_CTRL_ID2CLASS(id) == V4L2_CTRL_CLASS_USER )
    {
        // Standard control interface
        struct v4l2_control ctrl;
        ctrl.id = id;
        ctrl.value = value;
        return ( 0 == xioctl(fd, VIDIOC_S_CTRL, &ctrl) );
    }
    else
    {
        // Extended control interface
        struct v4l2_ext_control ctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        struct v4l2_ext_controls ctrls;
        memset(&ctrls, 0, sizeof(ctrls));

        ctrl.id = id;
        ctrl.value = value;

        ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(ctrl.id);
        ctrls.count = 1;
        ctrls.controls = &ctrl;
        return ( 0 == xioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls) );
    }
}

bool V4L2Wrapper::setControl(const std::string& name, std::int32_t value)
{
    if( cameraControls.find(name) == cameraControls.end() )
    {
        return false;
    }
    return setControl(cameraControls[name].id, value);
}

bool V4L2Wrapper::setCameraSettings(const lms::type::ModuleConfig *cameraConfig) {
    for( auto it = cameraControls.begin(); it != cameraControls.end(); ++it )
    {
        const std::string& name = it->first;
        const struct v4l2_queryctrl& ctrl = it->second;

        if( V4L2_CTRL_TYPE_BUTTON == ctrl.type )
        {
            // Do not set settings for buttons.. makes no sense
            continue;
        }

        if( ctrl.flags & (V4L2_CTRL_FLAG_DISABLED|V4L2_CTRL_FLAG_READ_ONLY) )
        {
            // Read-only control -> skip
            continue;
        }

        if( name == std::string("Trigger") )
        {
            // Skip trigger-settings -> should be set by regular config variable for proper trigger-handling
            continue;
        }

        int value;
        if(cameraConfig->hasKey(name)) {
            value = cameraConfig->get<int>(name);
        } else {
            value = ctrl.default_value;
        }

        setControl(ctrl.id, value);
        int actualValue = getControl(ctrl.id);
        if(actualValue != value) {
            // Configured and actual value differ..
            std::cerr << "[V4L2] Unable to set control '" << name
                                             << "' to desired value " << value
                                             << " (actual: " << actualValue << ")!";
        }
    }
    return true;
}

bool V4L2Wrapper::queryCameraControls()
{
    struct v4l2_queryctrl qctrl;
    memset(&qctrl, 0, sizeof(qctrl));

    qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == xioctl(fd, VIDIOC_QUERYCTRL, &qctrl)) {
        if( V4L2_CTRL_ID2CLASS(qctrl.id) != V4L2_CTRL_CLASS_USER   &&
            V4L2_CTRL_ID2CLASS(qctrl.id) != V4L2_CTRL_CLASS_CAMERA    )
        {
            // Ignore non-user/non-camera controls
            qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            continue;
        }

        if(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
        {
            // ignore disabled controls
            qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
            continue;
        }

        // Add control to list of supported controls
        cameraControls[ std::string((char*)qctrl.name) ] = qctrl;

        qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    return true;
}

bool V4L2Wrapper::printCameraControls()
{
    printf("Camera Controls:\n");
    for( auto it = cameraControls.begin(); it != cameraControls.end(); ++it )
    {
        const struct v4l2_queryctrl* qctrl = &it->second;
        switch( qctrl->type )
        {
            case V4L2_CTRL_TYPE_INTEGER:
                printf("  %s (int): value=%d min=%d max=%d step=%d default=%d\n",
                    qctrl->name, getControl(qctrl->id),
                    (int)qctrl->minimum, (int)qctrl->maximum,
                    (int)qctrl->step, (int)qctrl->default_value
                );
                break;
            // case V4L2_CTRL_TYPE_INTEGER_MENU:
            case V4L2_CTRL_TYPE_MENU:
                printf("  %s (menu): value=%d min=%d max=%d step=%d default=%d\n",
                    qctrl->name, getControl(qctrl->id),
                    (int)qctrl->minimum, (int)qctrl->maximum,
                    (int)qctrl->step, (int)qctrl->default_value
                );
                break;
            case V4L2_CTRL_TYPE_BOOLEAN:
                printf("  %s (bool): value=%d default=%d\n",
                    qctrl->name, getControl(qctrl->id),
                    (int)qctrl->default_value
                );
                break;
            case V4L2_CTRL_TYPE_BUTTON:
                printf("  %s (button)\n", qctrl->name);
                break;
            default:
                printf("  %s (UNKNOWN): value=%d min=%d max=%d step=%d default=%d\n",
                    qctrl->name, getControl(qctrl->id),
                    (int)qctrl->minimum, (int)qctrl->maximum,
                    (int)qctrl->step, (int)qctrl->default_value
                );
                break;
        }
    }

    return true;
}

const std::vector<Camera::Settings>& V4L2Wrapper::getValidCameraSettings() {
    if(cachedValidSettings.empty()) {
        getSupportedResolutions(cachedValidSettings);
    }

    return cachedValidSettings;
}

void V4L2Wrapper::getSupportedResolutions(std::vector<Settings> &result) {
    v4l2_fmtdesc desc;
    memset(&desc, 0, sizeof(desc));

    desc.index = 0;
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while(xioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0) {
        Camera::Settings res;
        std::string description = (char*)desc.description;

//        if(desc.flags & V4L2_FMT_FLAG_COMPRESSED) {
//            std::cout << "  COMPRESSED" << std::endl;
//        }
//        if(desc.flags & V4L2_FMT_FLAG_EMULATED) {
//            std::cout << "  EMULATED" << std::endl;
//        }
        getSupportedFramesizes(result, description, desc.pixelformat);
        desc.index ++;
    }
}

void V4L2Wrapper::getSupportedFramesizes(std::vector<Settings> &result,
                                         const std::string &description, std::uint32_t pixelFormat) {
    v4l2_frmsizeenum frm;
    memset(&frm, 0, sizeof(frm));

    frm.index = 0;
    frm.pixel_format = pixelFormat;

    xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frm);
    if(frm.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        while(xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frm) != -1) {
            getSupportedFramerates(result, description, pixelFormat,
                                   frm.discrete.width, frm.discrete.height);
            frm.index ++;
        }
    }
    int stepWidth = 0, stepHeight = 0;
    if(frm.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
        stepWidth = 1;
        stepHeight = 1;
    }
    if(frm.type == V4L2_FRMSIZE_TYPE_CONTINUOUS || frm.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
        int minW = frm.stepwise.min_width, minH = frm.stepwise.min_height;
        int maxW = frm.stepwise.max_width, maxH = frm.stepwise.max_height;
        if(stepWidth == 0) {
            stepWidth = frm.stepwise.step_width;
        }
        if(stepHeight == 0) {
            stepHeight = frm.stepwise.step_height;
        }

        for(int w = minW, h = minH; w <= maxW && h <= maxH; w+=stepWidth, h+=stepHeight) {
            getSupportedFramerates(result, description, pixelFormat, w, h);
        }
    }
}

void V4L2Wrapper::getSupportedFramerates(std::vector<Settings> &result,
                                         const std::string &description, std::uint32_t pixelFormat,
                                         std::uint32_t width, std::uint32_t height) {
    // http://guido.vonrudorff.de/v4l2-get-framerates/

    Settings settings;
    settings.description = description;
    settings.format = fromV4L2(pixelFormat);
    settings.width = width;
    settings.height = height;

    struct v4l2_frmivalenum temp;
    memset(&temp, 0, sizeof(temp));
    temp.pixel_format = pixelFormat;
    temp.width = width;
    temp.height = height;

    xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &temp);
    if (temp.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
        while (xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &temp) != -1) {
            settings.framerate = float(temp.discrete.denominator)/temp.discrete.numerator;
            result.push_back(settings);

            temp.index += 1;
        }
    }
    float stepval = 0;
    if (temp.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
        stepval = 1;
    }
    if (temp.type == V4L2_FRMIVAL_TYPE_STEPWISE || temp.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
        float minval = float(temp.stepwise.min.numerator)/temp.stepwise.min.denominator;
        float maxval = float(temp.stepwise.max.numerator)/temp.stepwise.max.denominator;
        if (stepval == 0) {
            stepval = float(temp.stepwise.step.numerator)/temp.stepwise.step.denominator;
        }
        for (float cval = minval; cval <= maxval; cval += stepval) {
            settings.framerate = 1 / cval;
            result.push_back(settings);
        }
    }
}

bool V4L2Wrapper::captureImage(lms::imaging::Image &image) {
    if(ioType == V4L2_CAP_READWRITE) {
        if(read(fd, image.data(), image.size()) == image.size()) {
            return success();
        } else {
            return error("Could not read full image");
        }
    } else if(ioType == V4L2_CAP_STREAMING) {

        v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        std::cout << "Before DQBUF" << std::endl;

        /* Dequeue */
        if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
            return perror("VIDIOC_DQBUF");
        }

        /* Copy data to image */
        std::cout << "Image: " << image.size() << " " << buffers[buf.index].length;

        lms::extra::PrecisionTime timestamp =
            lms::extra::PrecisionTime::fromMicros(buf.timestamp.tv_sec * 1000 * 1000 + buf.timestamp.tv_usec);

        std::cout << lms::extra::PrecisionTime::now() - timestamp;

        std::cout << "Before memcpy" << std::endl;

        memcpy(image.data(), buffers[buf.index].start, image.size());

        std::cout << "Before QBUF" << std::endl;

        /* Queue buffer for next frame */
        if(-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
            return perror("VIDIOC_QBUF");
        }

        return success();
    } else {
        return error("Wrong ioType");
    }
}

bool V4L2Wrapper::initBuffers() {
    // http://events.linuxfoundation.org/sites/events/files/slides/slides_4.pdf
    // http://linuxtv.org/downloads/v4l-dvb-apis/mmap.html

    v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(reqbuf));

    reqbuf.count = 20;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    if(-1 == xioctl(fd, VIDIOC_REQBUFS, &reqbuf)) {
        return perror("VIDIOC_REQBUFS");
    }

    // allocate buffers
    buffers = reinterpret_cast<MapBuffer*>(calloc(reqbuf.count, sizeof(*buffers)));
    numBuffers = reqbuf.count;

    std::cout<< "Num Buffers: " << numBuffers << std::endl;

    for(unsigned int n = 0; n < reqbuf.count; n++) {
        // query buffers
        struct v4l2_buffer buffer;

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = n;

        if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buffer)) {
            // unmap and free all previous buffers
            for(unsigned int i = 0; i < n; i++) {
                munmap(buffers[i].start, buffers[i].length);
            }

            return perror("VIDIOC_QUERYBUF");
        }
        // save length and start of each buffer
        buffers[n].length = buffer.length;
        buffers[n].start = mmap(NULL, buffer.length,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            fd, buffer.m.offset);

        if(MAP_FAILED == buffers[n].start) {
            /* If you do not exit here you should unmap() and free()
               the buffers mapped so far. */
            for(unsigned int i = 0; i < n; i++) {
                munmap(buffers[i].start, buffers[i].length);
            }

            return perror("MAP_FAILED");
        }
    }

    return success();
}

bool V4L2Wrapper::queueBuffers() {
    // enqueue all buffers
    for(unsigned int i = 0; i < numBuffers; ++i) {
        v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if(-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
            return perror("VIDIOC_QBUF");
        }
    }

    // start streaming
    std::uint32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(fd, VIDIOC_STREAMON, &type)) {
        return perror("VIDIOC_STREAMON");
    }

    return success();
}

bool V4L2Wrapper::destroyBuffers() {
    // stop streaming
    std::uint32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(fd, VIDIOC_STREAMOFF, &type)) {
        return perror("VIDIOC_STREAMOFF");
    }

    for (unsigned int i = 0; i < numBuffers; i++) {
        if(-1 == munmap(buffers[i].start, buffers[i].length)) {
            return perror("MUNMAP");
        }
    }

    free(buffers);

    return true;
}

}  // namespace lms_camera_importer
