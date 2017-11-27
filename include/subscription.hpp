#pragma once
#include <iostream> // std::cout

#include <common.hpp>

enum Subscription_Type : int {
  ASSOCIATED_ONLY = 0, // be notified only when data has been associated
  CAMERA_ONLY, // be notified only of camera updates
  MAVLINK_ONLY, // be notified only of MAVLink updates
  ALL_UPDATES // be notified only of all updates
};


class Subscription {
    Subscription(int sub_type, std::function<void(std::shared_ptr<AS_Data>)> cb) :
        _type(sub_type),
        _cb(cb)
    {
    }

    inline const int get_type(void) {return _type;}
    inline const std::function<void(std::shared_ptr<AS_Data>)> get_callback(void) {return _cb;}
protected:
    const int _type;
    const std::function<void(std::shared_ptr<AS_Data>)> _cb;
};
