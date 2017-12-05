#include <consumer.hpp>

using namespace std::placeholders; // for _1 in bind
using namespace moodycamel; // for lockless multi thread queue

void Consumer::callback(std::shared_ptr<AS_Data> sp) {
    // this function is called from within the Associator thread when the subscription requirements are met
    bool succeeded = _data_queue.try_enqueue(std::move(sp)); // TODO is std::move() the correct option here?
    if (!succeeded) {
        // the queue is full, either increase depth or drop the data
        // TODO handle failure
    }
}

void Consumer::register_callback(void) {
    // register this Consumers callback with the Associator
    _cc_associator->register_consumer_callback(std::bind(&Consumer::callback, this, _1));
}

void Consumer::run(void) {
    std::shared_ptr<AS_Data> sp;
    while (!_stop) {
        // block while attempting to get an item from the queue
        if ((_data_queue.wait_dequeue_timed(sp, std::chrono::milliseconds(_max_queue_block_ms)))) {
            // a callback event has occurred, we have a new item from the queue
            std::cout << "consumer got!" << sp << std::endl << std::endl;

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
