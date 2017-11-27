#pragma once
#include <iostream> // std::cout

#include <opencv2/core.hpp>
#include <m3api/xiApi.h>

#include <common.hpp>

class Camera_Data : public Data {
public:
    Camera_Data(XI_IMG xi_img, uint64_t capture_time, uint64_t feedback_time) :
        Data(DT_CAMERA)
    {
        cv::Mat(xi_img.height, xi_img.width, CV_8UC1, xi_img.bp).copyTo(image);
        timestamp_s = xi_img.tsSec;
        acq_nframe =xi_img.acq_nframe;
        nframe = xi_img.nframe;
        feedback_cc_us = capture_time;
        trigger_cc_us = feedback_time;
        timestamp_us = xi_img.tsUSec; // does not seem to work with our cameras (remains constant)
        std::cout << "constructed Camera_Data!" << timestamp_s << std::endl;
    }

    ~Camera_Data() {
        std::cout << "cleaned up Camera_Data!" << timestamp_us << std::endl;
    }

    unsigned int acq_nframe;
    unsigned int nframe;
    unsigned int timestamp_us;
    uint64_t feedback_cc_us;
    uint64_t trigger_cc_us;
    unsigned int timestamp_s;
    cv::Mat image;
};
