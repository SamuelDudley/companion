#pragma once
#include <iostream> // std::cout

#include <common.hpp>

class Attitude_Data :public Data {
public:
    Attitude_Data(mavlink_camera_feedback_ahrs_t camera_feedback_ahrs, uint64_t feedback_time) :
        Data(DT_ATTITUDE)
    {
        msg = camera_feedback_ahrs;
        feedback_cc_us = feedback_time;

    }

    ~Attitude_Data() {
    }

    mavlink_camera_feedback_ahrs_t msg;
    uint64_t feedback_cc_us;
};
