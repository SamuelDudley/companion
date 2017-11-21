#pragma once

#include <vector>
#include <functional>
#include <iostream>
#include <atomic>
#include <thread>
#include <memory> // smart_ptr
#include <chrono>
#include <mutex>

#include <readerwriterqueue.h>
#include <atomicops.h>

#include <opencv2/core.hpp>
//#include <mavlink/ardupilotmega/mavlink.h>

#ifndef ASSOCIATE_H
#define ASSOCIATE_H

class AS_Data {
    /*
     * this needs to contain the following:
     * the data that has been associated
     * the consumers that have subscribed to this data
     * the consumers that have been notified of this data update
     */
public:
    AS_Data(int a) {
       _a = a;
       std::cout << "constructed DATA!" << _a << std::endl;
    }

    ~AS_Data() {
        std::cout << "cleaned up DATA!" << _a << std::endl;
    }

    int _a;
};


class Associator {
public:
    // A struct containing associated data entries
    struct Container {
        int mavlink_data;
        int camera_data;
    };

    using cb1_t = std::function<void(std::shared_ptr<AS_Data>)>;
    using callbacks_t = std::vector<cb1_t>;

    // a vector of callback functions
    callbacks_t callbacks;

    void invoke_callbacks(void); // calls all registered callbacks
    void register_callback(const cb1_t &cb);
    void create_new_container(void);
};

// when a Consumer object is created we want it to register its callback fn with the Associator object
class Consumer {
public:
    Consumer(Associator *obj_associator, int idx) :
        q(100),
        _stop(false),
        _callback_handled_flag(ATOMIC_FLAG_INIT)
    {
        _cc_associator = obj_associator;
        _idx = idx;
        register_callback();
        start();
    }

    ~Consumer() {
        try {
            stop();
        } catch(...) {
            /* thread.join() threw an exception..
             * handle it here or force kill?
             */
        }
        // any shared_ptr's in data_pointers will be cleaned up here...
    }

    moodycamel::BlockingReaderWriterQueue<std::shared_ptr<AS_Data>> q;

    void callback(std::shared_ptr<AS_Data> sp);
    void stop(void);


protected:
    Associator *_cc_associator; // pointer to associator object
    void register_callback(void); // registers this consumers callback with the associator
    int _temp;
    int _idx;

    void start(void);
    void run(void);

    std::atomic_bool _stop;

    std::thread _thread;
};

#endif // ASSOCIATE_H
