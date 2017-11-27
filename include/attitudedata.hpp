#pragma once
#include <iostream> // std::cout

#include <common.hpp>

class Attitude_Data :public Data {
public:
    Attitude_Data(mavlink_heartbeat_t heartbeat, uint64_t feedback_time) :
        Data(DT_ATTITUDE)
    {
        msg = heartbeat;
        feedback_cc_us = feedback_time;

    }

    ~Attitude_Data() {
    }

    mavlink_heartbeat_t msg;
    uint64_t feedback_cc_us;
};
