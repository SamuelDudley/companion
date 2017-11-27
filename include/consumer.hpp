#pragma once

#include <atomic>
#include <thread>
#include <memory> // smart_ptr

#include <readerwriterqueue.h>
#include <atomicops.h>

#include <associate.hpp>

// when a Consumer object is created we want it to register its callback fn with the Associator object
class Consumer {
public:
    Consumer(std::shared_ptr<Associator> obj_associator, int idx) :
        _data_queue(100), // assign memory for a queue 100 objects deep
        _max_queue_block_ms(10),
        _stop(false)
    {
        _cc_associator = obj_associator;
        _idx = idx;
        register_callback();
    }

    ~Consumer() {
        try {
            stop();
        } catch(...) {
            /* thread.join() threw an exception..
             * handle it here or force kill?
             */
        }
    }

    void callback(std::shared_ptr<AS_Data> sp);

    void start(void);
    void stop(void);

protected:
    std::shared_ptr<Associator> _cc_associator; // pointer to associator object

    moodycamel::BlockingReaderWriterQueue<std::shared_ptr<AS_Data>> _data_queue;
    int _max_queue_block_ms;

    void register_callback(void); // registers this consumers callback with the associator
    int _idx;

    void run(void);


    std::atomic_bool _stop;
    std::thread _thread;
};
