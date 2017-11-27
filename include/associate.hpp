#pragma once

#include <vector>
#include <functional>
#include <iostream>
#include <atomic>
#include <thread>
#include <memory> // smart_ptr
#include <chrono>
#include <mutex>

#include <blockingconcurrentqueue.h>

#include <opencv2/core.hpp>
//#include <mavlink/ardupilotmega/mavlink.h>

#include <cameradata.hpp>
#include <attitudedata.hpp>
#include <associateddata.hpp>
#include <common.hpp>

#ifndef ASSOCIATE_H
#define ASSOCIATE_H

class Associator {
public:
    Associator() :
        in_data_queue(100), // assign memory for an input queue 100 objects deep
        _stop(false),
        _max_in_queue_block_ms(10)
    {
        state = NOT_SYNCED;
        waiting_on_mavlink_data = true;
        waiting_on_camera_data = true;
        start();
    }

    ~Associator() {
        try {
            stop();
        } catch(...) {
            /* thread.join() threw an exception..
             * handle it here or force kill?
             */
        }
    }

    enum states : int {
        NOT_SYNCED = 0,
        SYNCING,
        IN_SYNC
    };

    int state;
    int img_idx_offset;

    using cb1_t = std::function<void(std::shared_ptr<AS_Data>)>;

    std::vector<cb1_t> _consumer_callbacks; // a vector of callback functions
    std::vector<std::shared_ptr<AS_Data>> associated_data_entries;
    std::shared_ptr<Data> tmp_img;
    void invoke_consumer_callbacks(void); // calls all registered callbacks
    void register_consumer_callback(const cb1_t &cb);
    void process_incoming(void);
    void create_new_container(void);

    moodycamel::BlockingConcurrentQueue<std::shared_ptr<Data>> in_data_queue;

private:
    void start(void);
    void run(void);
    void stop(void);

    std::atomic_bool _stop;
    std::thread _thread;
    int _max_in_queue_block_ms;

    std::atomic_bool waiting_on_mavlink_data;
    std::atomic_bool waiting_on_camera_data;


};

#endif // ASSOCIATE_H
