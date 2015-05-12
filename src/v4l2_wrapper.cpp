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

V4L2Wrapper::V4L2Wrapper(lms::logging::Logger *rootLogger) : logger("V4L2", rootLogger), fd(0), ioType(0) {
}

bool V4L2Wrapper::openDevice(const std::string &devicePath) {
    this->devicePath = devicePath;
    fd = ::open(devicePath.c_str(), O_RDWR /* O_RDONLY */);

    if(fd == -1) {
        logger.error("openDevice") << "Could not open Camera Device " << strerror(errno);
        fd = 0;
        return false;
    }

    if(!isValidCamera()) {
        return false;
    }

    return true;
}

bool V4L2Wrapper::initBuffersIfNecessary() {
    if(ioType == V4L2_CAP_STREAMING) {
        logger.info("openDevice") << "Init MEMORY MAPPING";

        if(! initBuffers()) {
            return false;
        }

        if(! queueBuffers()) {
            return false;
        }
    }

    return true;
}

bool V4L2Wrapper::closeDevice() {
    if(fd != 0) {
        if(ioType == V4L2_CAP_STREAMING) {
            destroyBuffers();
        }

        if(close(fd) == -1) {
            logger.error("closeDevice") << "Could not close Camera Device " << strerror(errno);
            fd = 0;
            return false;
        }

        fd = 0;
    }

    return true;
}

bool V4L2Wrapper::isOpen() {
    return fd != 0;
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

bool V4L2Wrapper::setFormat(std::uint32_t width, std::uint32_t height, lms::imaging::Format fmt) {
    // http://linuxtv.org/downloads/v4l-dvb-apis/vidioc-g-fmt.html

    // get current pixel format of camera
    v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl (fd, VIDIOC_G_FMT, &format)) {
        logger.error("setPixelFormat") << "During VIDIOC_G_FMT: " << strerror(errno);
        return false;
    }

    // Set new values
    int bytesPerPixel = lms::imaging::bytesPerPixel(fmt);

    if(bytesPerPixel <= 0) {
        logger.error("setFormat") << "Bytes per pixel is " << bytesPerPixel;
        return false;
    }

    // http://linuxtv.org/downloads/v4l-dvb-apis/pixfmt.html#idp22265936
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = toV4L2(fmt);
    //format.fmt.pix.bytesperline = width * bytesPerPixel;
    //format.fmt.pix.sizeimage = lms::imaging::imageBufferSize(width, height, fmt);

    // try to set new values
    if (-1 == xioctl (fd, VIDIOC_S_FMT, &format)) {
        logger.error("setPixelFormat") << "During VIDIOC_S_FMT: " << strerror(errno);
        return false;
    }

    // check if the settings are accepted
    if(format.fmt.pix.width != width || format.fmt.pix.height != height
            || format.fmt.pix.pixelformat != toV4L2(fmt)) {

        logger.error("setPixelFormat") << "Could not set width/height/pixelformat";
        return false;
    }

    return true;
}

bool V4L2Wrapper::setFramerate(std::uint32_t framerate) {
    v4l2_streamparm streamparm;
    v4l2_fract *tpf;
    memset (&streamparm, 0, sizeof (streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tpf = &streamparm.parm.capture.timeperframe;
    tpf->numerator = 1;
    tpf->denominator = framerate;

    if (xioctl(fd, VIDIOC_S_PARM, &streamparm) == -1) {
        logger.error("setFramerate") << "Failed to set camera FPS: " << strerror(errno);
        return false;
    }

    return true;
}

std::uint32_t V4L2Wrapper::getFramerate() {
    v4l2_streamparm streamparm;
    memset (&streamparm, 0, sizeof (streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_G_PARM, &streamparm) == -1) {
        logger.error("getFramerate") << "Failed to get camera FPS: " << strerror(errno);
        return 0;
    }

    if(! (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        logger.warn("getFramerate") << "Camera does not support FPS setting";
    }

    v4l2_fract *tpf = &streamparm.parm.capture.timeperframe;

    return tpf->denominator / tpf->numerator;
}

bool V4L2Wrapper::isValidCamera() {
    struct v4l2_capability cap;
    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            logger.error("checkCameraFileHandle") << "No V4L2 device " << strerror(errno);
        } else {
            logger.error("checkCameraFileHandle") << "Error in ioctl VIDIOC_QUERYCAP " << strerror(errno);
        }
        return false;
    }

    //logger.info("isValidCamera") << cap.driver << " " << cap.card << " " << cap.bus_info;

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        logger.error("checkCameraFileHandle") << "is no video capture device " << strerror(errno);
        return false;
    }

    if(cap.capabilities & V4L2_CAP_STREAMING) {
        logger.debug("checkCameraFileHandle") << "supports streaming API";
        ioType = V4L2_CAP_STREAMING;
    } else if (cap.capabilities & V4L2_CAP_READWRITE) {
        logger.debug("checkCameraFileHandle") << "supports read IO";
        ioType = V4L2_CAP_READWRITE;
    } else {
        logger.error("isValidCamera") << "does not support any IO operations";
        ioType = 0;
    }

    return true;
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
            logger.warn("setCameraSettings") << "[V4L2] Unable to set control '" << name
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

void V4L2Wrapper::getSupportedResolutions(std::vector<CameraResolution> &result) {
    v4l2_fmtdesc desc;
    memset(&desc, 0, sizeof(desc));

    desc.index = 0;
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while(xioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0) {
        CameraResolution res;
        res.pixelFormat = std::string((char*)desc.description);
        res.internalPixelFormat = desc.pixelformat;

//        if(desc.flags & V4L2_FMT_FLAG_COMPRESSED) {
//            std::cout << "  COMPRESSED" << std::endl;
//        }
//        if(desc.flags & V4L2_FMT_FLAG_EMULATED) {
//            std::cout << "  EMULATED" << std::endl;
//        }
        getSupportedFramesizes(result, res);
        desc.index ++;
    }
}

void V4L2Wrapper::getSupportedFramesizes(std::vector<CameraResolution> &result, CameraResolution res) {
    v4l2_frmsizeenum frm;
    memset(&frm, 0, sizeof(frm));

    frm.index = 0;
    frm.pixel_format = res.internalPixelFormat;

    xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frm);
    if(frm.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        while(xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frm) != -1) {
            res.width = frm.discrete.width;
            res.height = frm.discrete.height;
            getSupportedFramerates(result, res);
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
            res.width = w;
            res.height = h;
            getSupportedFramerates(result, res);
        }
    }
}

void V4L2Wrapper::getSupportedFramerates(std::vector<CameraResolution> &result,
                                         CameraResolution res) {
    // http://guido.vonrudorff.de/v4l2-get-framerates/

    struct v4l2_frmivalenum temp;
    memset(&temp, 0, sizeof(temp));
    temp.pixel_format = res.internalPixelFormat;
    temp.width = res.width;
    temp.height = res.height;

    xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &temp);
    if (temp.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
        while (xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &temp) != -1) {
            res.framerate = float(temp.discrete.denominator)/temp.discrete.numerator;
            result.push_back(res);

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
            res.framerate = 1 / cval;
            result.push_back(res);
        }
    }
}

bool V4L2Wrapper::captureImage(lms::imaging::Image &image) {
    if(ioType == V4L2_CAP_READWRITE) {
        return read(fd, image.data(), image.size()) == image.size();
    } else if(ioType == V4L2_CAP_STREAMING) {

        v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        /* Dequeue */
        if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
            logger.error("captureImage") << "VIDIOC_DQBUF " << strerror(errno);
            return false;
        }

        /* Copy data to image */
        logger.info("captureImage") << "Image: " << image.size() << " " << buffers[buf.index].length;

        //timeval now;

        lms::extra::PrecisionTime timestamp =
            lms::extra::PrecisionTime::fromMicros(buf.timestamp.tv_sec * 1000 * 1000 + buf.timestamp.tv_usec);

        logger.info("delay") << lms::extra::PrecisionTime::now() - timestamp;

        memcpy(image.data(), buffers[buf.index].start, image.size());

        /* Queue buffer for next frame */
        if(-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
            logger.error("captureImage") << "VIDIOC_QBUF " << strerror(errno);
            return false;
        }

        return true;
    } else {
        logger.error("captureImage") << "Wrong ioType";
        return false;
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
        logger.error("initBuffers") << "VIDIOC_REQBUFS " << strerror(errno);
        return false;
    }

    // allocate buffers
    buffers = reinterpret_cast<MapBuffer*>(calloc(reqbuf.count, sizeof(*buffers)));
    numBuffers = reqbuf.count;

    logger.info("initBuffers") << "Number of buffers: " << numBuffers;

    for(unsigned int n = 0; n < reqbuf.count; n++) {
        // query buffers
        struct v4l2_buffer buffer;

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = n;

        if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buffer)) {
            logger.error("initBuffers") << "VIDIOC_QUERYBUF " << strerror(errno);

            // unmap and free all previous buffers
            for(unsigned int i = 0; i < n; i++) {
                munmap(buffers[i].start, buffers[i].length);
            }

            return false;
        }
        // save length and start of each buffer
        buffers[n].length = buffer.length;
        buffers[n].start = mmap(NULL, buffer.length,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            fd, buffer.m.offset);

        if(MAP_FAILED == buffers[n].start) {
            logger.error("initBuffers") << "MAP_FAILED " << strerror(errno);

            /* If you do not exit here you should unmap() and free()
               the buffers mapped so far. */
            for(unsigned int i = 0; i < n; i++) {
                munmap(buffers[i].start, buffers[i].length);
            }

            return false;
        }
    }

    return true;
}

bool V4L2Wrapper::queueBuffers() {
    for(unsigned int i = 0; i < numBuffers; ++i) {
        v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if(-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
            logger.error("queueBuffers") << "Failed";
            return false;
        }
    }

    std::uint32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == xioctl(fd, VIDIOC_STREAMON, &type)) {
        logger.error("queueBuffers") << "Failed";
        return false;
    }

    return true;
}

bool V4L2Wrapper::destroyBuffers() {
    for (unsigned int i = 0; i < numBuffers; i++) {
        munmap(buffers[i].start, buffers[i].length);
    }

    free(buffers);

    return true;
}

}  // namespace lms_camera_importer
