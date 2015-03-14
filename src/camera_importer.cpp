#include <camera_importer.h>
#include <cstdint>
#include <string>
#include <lms/type/static_image.h>
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

static int xioctl(int64_t fh, int64_t request, void *arg)
{
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

bool CameraImporter::initialize() {
    logger.info() << "Init: CameraImporter";

    // read config
    cameraConfig = getConfig();

    file = cameraConfig->get<std::string>("device");
    width = cameraConfig->get<int>("width");
    height = cameraConfig->get<int>("height");
    bpp = cameraConfig->get<int>("bpp");
    framerate = cameraConfig->get<int>("framerate");

    // initialize image
    bufsize = width * height * bpp;
    cameraBuffer = new std::uint8_t[bufsize];

	//Start Camera
	///Set All stuff
    //TODO Don't know what that command should do!
    std::string cmd = "v4l2-ctl -d " + file + " --set-fmt-video=width=" + std::to_string(width) + ",height=" + std::to_string(height) + ",pixelformat=YUYV -p" + std::to_string(framerate);
    logger.debug("init") << "Executing " << cmd;

    int cmdStatus = system(cmd.c_str());
    if(cmdStatus != 0) {
        logger.warn("init") << "Command returned with " << cmdStatus;
    }
    logger.debug("init") << "Opening: " << file;


    fflush(stdout);  // TODO why do we need that here?

    fd_camera = open(file.c_str(), O_RDONLY);
    checkCameraFileHandle();

    logger.info("camera was set up!");
    
    // Set camera settings

    queryCameraControls();
    setCameraSettings();
    queryCameraControls(); // Re-read current controls
    printCameraControls();

    logger.info() << "After query and set!!";

	return true;
}

bool CameraImporter::deinitialize() {
    logger.info("deinit") << "Deinit: CameraImporter";
	//Stop Camera
	if (fd_camera)
        close (fd_camera);
	return true;
}

bool CameraImporter::cycle () {
    if (fd_camera == 0) return false;
    if (fd_camera != 0) return false; //TODO remove

    //Read Camera
    bool valid = checkCameraFileHandle();

    //TODO Nicht so geil
    if(!valid) {
        printf("Camera Importer: Camera handle not valid!\n");
        while(!valid) {
            close(fd_camera);
            std::string cmd = "v4l2-ctl -d " + file + " --set-fmt-video=width=" + std::to_string(width) + ",height=" + std::to_string(height) + ",pixelformat=YUYV -p" + std::to_string(framerate);
            system(cmd.c_str());
            fd_camera = open(file.c_str(), O_RDONLY);
            valid = checkCameraFileHandle();

            usleep(100);
        }
        // Set camera settings

        queryCameraControls();
        setCameraSettings();
        queryCameraControls(); // Re-read current controls
        printCameraControls();

    }

    read(fd_camera, cameraBuffer, bufsize);
    //TODO set camera-data-channel

	return true;
}

bool CameraImporter::setCameraSettings(){
    /*
    auto config = datamanager()->config((std::string("camera_settings_") + cameraSettingsFile).c_str());
    
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
        
        int value = config->get_or_default<int>(name, ctrl.default_value);
        
        setControl<int>(ctrl.id, value);
        int actualValue = getControl<int>(ctrl.id);
        if( actualValue != value )
        {
            // Configured and actual value differ..
            printf("[V4L2] Unable to set control '%s' to desired value %i (actual: %i)!\n", name.c_str(), value, actualValue);
        }
    }
    */
    return true;
}

bool CameraImporter::queryCameraControls()
{
    struct v4l2_queryctrl qctrl;
    memset(&qctrl, 0, sizeof(qctrl));
    
    qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == xioctl(fd_camera, VIDIOC_QUERYCTRL, &qctrl)) {
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

bool CameraImporter::printCameraControls()
{
    printf("Camera Controls:\n");
    for( auto it = cameraControls.begin(); it != cameraControls.end(); ++it )
    {
        const struct v4l2_queryctrl* qctrl = &it->second;
        switch( qctrl->type )
        {
            case V4L2_CTRL_TYPE_INTEGER:
                printf("  %s (int): value=%d min=%d max=%d step=%d default=%d\n",
                    qctrl->name, getControl<int>(qctrl->id),
                    (int)qctrl->minimum, (int)qctrl->maximum,
                    (int)qctrl->step, (int)qctrl->default_value
                );
                break;
            // case V4L2_CTRL_TYPE_INTEGER_MENU:
            case V4L2_CTRL_TYPE_MENU:
                printf("  %s (menu): value=%d min=%d max=%d step=%d default=%d\n",
                    qctrl->name, getControl<int>(qctrl->id),
                    (int)qctrl->minimum, (int)qctrl->maximum,
                    (int)qctrl->step, (int)qctrl->default_value
                );
                break;
            case V4L2_CTRL_TYPE_BOOLEAN:
                printf("  %s (bool): value=%d default=%d\n",
                    qctrl->name, getControl<int>(qctrl->id),
                    (int)qctrl->default_value
                );
                break;
            case V4L2_CTRL_TYPE_BUTTON:
                printf("  %s (button)\n", qctrl->name);
                break;
            default:
                printf("  %s (UNKNOWN): value=%d min=%d max=%d step=%d default=%d\n",
                    qctrl->name, getControl<int>(qctrl->id),
                    (int)qctrl->minimum, (int)qctrl->maximum,
                    (int)qctrl->step, (int)qctrl->default_value
                );
                break;
        }
    }
    
    return true;
}

template<typename T>
T CameraImporter::getControl(uint32_t id)
{
    if( V4L2_CTRL_ID2CLASS(id) == V4L2_CTRL_CLASS_USER )
    {
        // Standard control interface
        struct v4l2_control ctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        ctrl.id = id;
        xioctl(fd_camera, VIDIOC_G_CTRL, &ctrl);
        return T(ctrl.value);
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
        
        xioctl(fd_camera, VIDIOC_G_EXT_CTRLS, &ctrls);
        
        // Supports only 32bit property values for now...
        return T(ctrl.value);
    }
}

template<typename T>
T CameraImporter::getControl(const std::string& name)
{
    if( cameraControls.find(name) == cameraControls.end() )
    {
        return false;
    }
    return getControl<T>(cameraControls[name].id);
}

template<typename T>
bool CameraImporter::setControl(uint32_t id, T value)
{
    if( V4L2_CTRL_ID2CLASS(id) == V4L2_CTRL_CLASS_USER )
    {
        // Standard control interface
        struct v4l2_control ctrl;
        ctrl.id = id;
        ctrl.value = value;
        return ( 0 == xioctl(fd_camera, VIDIOC_S_CTRL, &ctrl) );
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
        return ( 0 == xioctl(fd_camera, VIDIOC_S_EXT_CTRLS, &ctrls) );
    }
}

template<typename T>
bool CameraImporter::setControl(const std::string& name, T value)
{
    if( cameraControls.find(name) == cameraControls.end() )
    {
        return false;
    }
    return setControl(cameraControls[name].id, value);
}

bool CameraImporter::checkCameraFileHandle() {  
    struct v4l2_capability cap;
    if (-1 == xioctl (fd_camera, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            logger.error("checkCameraFileHandle") << "No V4L2 device " << strerror(errno);
        } else {
            logger.error("checkCameraFileHandle") << "Error in ioctl VIDIOC_QUERYCAP " << strerror(errno);
        }
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        logger.error("checkCameraFileHandle") << "is no video capture device " << strerror(errno);
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
        logger.error("checkCameraFileHandle") << "does not support read i/o " << strerror(errno);
        return false;
    }

    return true;
}
