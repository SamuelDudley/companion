#include <camera.hpp>

void Camera::start(void) {
    // TODO: allow restart of the camera to change settings

    // Retrieving a handle to the camera device
    printf("Opening first camera...\n");
    stat = xiOpenDevice(0, &xiH);
    HandleResult(stat,"xiOpenDevice");

    // Note:
    // The default parameters of each camera might be different in different API versions
    // In order to ensure that your application will have camera in expected state,
    // please set all parameters expected by your application to required value.

    // Setting "exposure" parameter (10ms=10000us)
    stat = xiSetParamInt(xiH, XI_PRM_EXPOSURE, 10000);
    HandleResult(stat,"xiSetParam (exposure set)");

    // set the output of the camera to trigger on frame exposure
    xiSetParamInt(xiH, XI_PRM_GPO_SELECTOR, 1);
    xiSetParamInt(xiH, XI_PRM_GPO_MODE,  XI_GPO_EXPOSURE_PULSE); // XI_GPO_EXPOSURE_ACTIVE

    // set acquisition to frame rate mode
    xiSetParamInt(xiH,XI_PRM_ACQ_TIMING_MODE, XI_ACQ_TIMING_MODE_FRAME_RATE);
    // set frame rate
    xiSetParamInt(xiH,XI_PRM_FRAMERATE,10);//60

    // set trigger mode to software
//    xiSetParamInt(xiH, XI_PRM_TRG_SOURCE, XI_TRG_SOFTWARE);


//    int width_inc;
//    xiGetParamInt(xiH, XI_PRM_HEIGHT XI_PRM_INFO_INCREMENT, &width_inc);
//    std::cout << width_inc << std::endl;

//  xiSetParamInt(xiH, XI_PRM_WIDTH, 1660);
//  xiSetParamInt(xiH, XI_PRM_HEIGHT, 1660);
//  xiSetParamInt(xiH, XI_PRM_OFFSET_Y, 218);
//  xiSetParamInt(xiH, XI_PRM_OFFSET_X, 218);

    // https://www.ximea.com/support/wiki/apis/XiAPI_Manual#XI_PRM_TRANSPORT_DATA_TARGET-or-transport_data_target
//  xiSetParamInt(xiH, XI_PRM_RECENT_FRAME, 1); // get most recent image from buffer

    start_capture();
}

void Camera::stop(void) {
    stop_capture();
    xiCloseDevice(xiH);
    std::cout << "Camera has exited!" << _idx << std::endl;
}

void Camera::stop_capture(void) {
    _stop = true;
    _thread.join(); // block here until the camera thread has completed
    printf("Stopping acquisition...\n");
    xiStopAcquisition(xiH);
}

void Camera::start_capture(void) {
    _stop = false;
    _thread = std::thread(&Camera::run, this);
}

void Camera::run(void) {
    std::cout << "Starting acquisition...\n" << std::endl;
    stat = xiStartAcquisition(xiH);
    HandleResult(stat,"xiStartAcquisition");

    // image buffer
    XI_IMG image;
    memset(&image,0,sizeof(image));
    image.size = sizeof(XI_IMG);

//    xiSetParamInt(xiH, XI_PRM_TRG_SOFTWARE, 1); // used to software trigger the camera
    uint64_t trigger_time = get_time_usec();
    while (!_stop) {
        // getting image from camera
        stat = xiGetImage(xiH, 5000, &image);
        HandleResult(stat,"xiGetImage");
        uint64_t capture_time = get_time_usec();
        if (stat == XI_OK) {
            // we have got a new image from the camera
            std::shared_ptr<Data> sp(std::make_shared<Camera_Data>(image, capture_time, trigger_time));
            bool succeeded = _cc_associator->in_data_queue.try_enqueue(std::move(sp));

            if (!succeeded) {
                std::cout << "image data not on queue!" << std::endl;
               // the queue is full, either increase depth, increase rate on other end or drop the data
               // TODO handle failure
            }

//            cv::Mat mat = cv::Mat(image.height, image.width, CV_8UC1, image.bp);
//            cv::imwrite( "/tmp/images/"+std::to_string(image_counter)+".jpg", mat);
//            cv::namedWindow( "corners1", 1 );
//            cv::imshow( "corners1", mat );
//            cv::waitKey();
//            cv::Mat B = mat.clone(); // deep copy of mat image... we can safely send this off to be processed
            _image_counter++;
            printf("Image %d (%dx%d) received from camera.\n", _image_counter, (long)image.tsUSec, (long)image.tsSec);
        } else {
            // no image received from camera
        }
    }
}
