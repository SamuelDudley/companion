#pragma once

#include <iostream>
#include <thread>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <m3api/xiApi.h>

#include <common.hpp>
#include <associate.hpp>

#include <cameradata.hpp>


#define HandleResult(res,place) if (res!=XI_OK) {printf("Error after %s (%d)\n",place,res);return;}

class Camera {
public:
    Camera(std::shared_ptr<Associator> obj_associator, int idx) :
        _stop(false),
         xiH(NULL),
         stat(XI_OK),
         _image_counter(0)
    {
        _cc_associator = obj_associator;
        _idx = idx;
    }

    ~Camera() {
        try {
            stop();
        } catch(...) {
            /* thread.join() threw an exception..
             * handle it here or force kill?
             */
        }
    }

    void start(void);
    void stop(void);
    void start_capture(void);
    void stop_capture(void);

protected:
    void run(void);

    std::shared_ptr<Associator> _cc_associator; // pointer to associator object
    int _idx;

    std::atomic_bool _stop;
    std::thread _thread;

    HANDLE xiH;
    XI_RETURN stat;
    int _image_counter;
};
