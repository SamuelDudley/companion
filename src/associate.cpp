/*
 * Data association module. Takes mavlink data and images to make a deep buffer to be accessed by consumers.
 * Consumers subscribe to this module during construction stating what infomation should be provided.
 */
#include <associate.hpp>
//using namespace std::chrono_literals;
using namespace std::placeholders; // for _1 in bind
using namespace moodycamel; // for ReaderWriterQueue


//Associator needs to get data from camera and mavlink then associate...
//Associator needs to keep sync...

void Associator::register_callback(const cb1_t &cb) {
    callbacks.push_back(cb);
}

void Associator::invoke_callbacks(void) {
    // Invoke the functions that have registered with this associator
    std::shared_ptr<AS_Data> sp(std::make_shared<AS_Data>(1)); // single memory allocation
    for(auto& fun : callbacks) {
        fun(sp); // pass the shared pointer to the consumers
    }
} // shared_ptr goes out of scope, so the only copies are now with the consumers

//void create_new_container(void) {
//    std::shared_ptr<AS_Data> p(std::make_shared<AS_Data>()); // single memory allocation
//
//}

/*
 * subscribed consumers all get a copy of the shared pointer
 * Once all the consumers derefrence the smart pointer the object is removed
 * p = nullptr; delete p;
 *
 * for thread safety consumers are not to write to the pointed object! read only!
 *
 */

void Consumer::callback(std::shared_ptr<AS_Data> sp) {
    // this function is called from the Associator when the subscription requirements are met
    bool succeeded = q.try_enqueue(sp);
    if (!succeeded) {
        // the queue is full, either increase depth or drop the data
    }
}

void Consumer::register_callback(void) {
    // register this Consumers callback with the Associator
    _cc_associator->register_callback(std::bind(&Consumer::callback, this, _1));
}

void Consumer::run(void) {
    // this runs in a thread
    std::shared_ptr<AS_Data> sp;
    while (!_stop) {
        // block while attempting to get an item from the queue
        if ((q.wait_dequeue_timed(sp, std::chrono::milliseconds(10)))) {
            // a callback event has occurred, we have a new item from the queue
            std::cout << "got!" << sp << std::endl;

            // do work with data here..

            // we are done with this smart pointer. de-reference
            sp = nullptr;
        }
    }
}

void Consumer::start(void) {
    _thread = std::thread(&Consumer::run, this);
}

void Consumer::stop(void) {
    _stop = true;
    _thread.join(); // block here until the consumer thread has completed
    std::cout << "Consumer has exited!" << _idx << std::endl;
}

