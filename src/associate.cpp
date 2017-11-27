/*
 * Data association module. Takes mavlink data and images associates them and pushes to consumers.
 * Consumers subscribe to this module during construction stating what infomation should be provided.
 *
 * subscribed consumers all get a copy of the shared pointer
 * Once all the consumers derefrence the smart pointer the object is removed
 *
 * for thread safety consumers are not to write to the pointed object! read only!
 *
 */

#include <associate.hpp>

using namespace moodycamel; // for lockless multi thread queue

void Associator::register_consumer_callback(const cb1_t &cb) {
    _consumer_callbacks.push_back(cb);
}

void Associator::process_incoming(void) {
    std::shared_ptr<Data> tmp;
    if (in_data_queue.wait_dequeue_timed(tmp, std::chrono::milliseconds(_max_in_queue_block_ms))) {
        // static_pointer_cast to go up class hierarchy
        // note it is possible to cast the wrong type here without error...
        // really need to make sure the type you are casting it to is correct!

        switch (tmp->get_type()) {
        case DT_ATTITUDE: {
            std::shared_ptr<Attitude_Data> data;
            data = std::static_pointer_cast<Attitude_Data>(tmp);

            std::cout << "mavlink:" << data->feedback_cc_us <<  std::endl;

            if (waiting_on_mavlink_data && !waiting_on_camera_data) {
                // we were waiting for mavlink to associate with an image
                // try to associate this mavlink data with the last AS_DATA element
                if (state == SYNCING) {
                    // calculate the counter / mavlink counter offset
                    unsigned int cam_img_idx = associated_data_entries.back()->cam->nframe;
                    img_idx_offset = data->msg.img_idx - cam_img_idx;
                    associated_data_entries.back()->msg = data;
                    associated_data_entries.back()->modified = true;
                    associated_data_entries.back()->complete = true;
                    state = IN_SYNC;
                    waiting_on_mavlink_data = false;

                } else if (state == IN_SYNC) {
                    unsigned int mav_img_idx = associated_data_entries.back()->cam->nframe;
                    if (data->msg.img_idx - mav_img_idx == img_idx_offset) {
                        associated_data_entries.back()->msg = data;
                        associated_data_entries.back()->modified = true;
                        associated_data_entries.back()->complete = true;
                        state = IN_SYNC;
                        waiting_on_mavlink_data = false;
                    } else {
                        // index offset between the camera and mavlink did not match
                        // we are out of sync
                        state = NOT_SYNCED;
                    }
                }

            } else if (waiting_on_mavlink_data && waiting_on_camera_data) {
                // we are waiting for anything... this only occurs if we are not yet in sync
                // make a new AS_DATA element
                std::shared_ptr<AS_Data> sp(std::make_shared<AS_Data>()); // single memory allocation
                sp->msg = data;
                sp->modified = true;
                associated_data_entries.push_back(sp);
                waiting_on_mavlink_data = false;
                state = SYNCING;

            } else if (!waiting_on_mavlink_data && !waiting_on_camera_data){
                // not waiting on anything... make a new data entry
                std::shared_ptr<AS_Data> sp(std::make_shared<AS_Data>()); // single memory allocation
                sp->msg = data;
                sp->modified = true;
                associated_data_entries.push_back(sp);
                waiting_on_camera_data = true;

            } else if ( !waiting_on_mavlink_data && waiting_on_camera_data) {
                // we were not expecting mavlink data to arrive
                // we are out of sync
                state = NOT_SYNCED;

            } else {
                // we should never get here...
                // TODO handle this case if we get here somehow
            }

            break;
        }
        case DT_CAMERA: {
            std::shared_ptr<Camera_Data> data;
            data = std::static_pointer_cast<Camera_Data>(tmp);

            std::cout << "image:" << data->timestamp_s << ' ' << data->nframe <<  std::endl;

            if (waiting_on_camera_data && !waiting_on_mavlink_data) {
                // we were waiting for camera data to associate with a mavlink message
                // try to associate this camera data with the last AS_DATA element
                if (state == SYNCING) {
                    // calculate the counter / mavlink counter offset
                    unsigned int mav_img_idx = associated_data_entries.back()->msg->msg.img_idx;
                    img_idx_offset = mav_img_idx - data->nframe;
                    associated_data_entries.back()->cam = data;
                    associated_data_entries.back()->modified = true;
                    associated_data_entries.back()->complete = true;
                    state = IN_SYNC;
                    waiting_on_camera_data = false;

                } else if (state == IN_SYNC) {
                    unsigned int mav_img_idx = associated_data_entries.back()->msg->msg.img_idx;
                    if (mav_img_idx - data->nframe == img_idx_offset) {
                        associated_data_entries.back()->cam = data;
                        associated_data_entries.back()->modified = true;
                        associated_data_entries.back()->complete = true;
                        state = IN_SYNC;
                        waiting_on_camera_data = false;
                    } else {
                        // index offset between the camera and mavlink did not match
                        // we are out of sync
                        state = NOT_SYNCED;
                    }
                }
            } else if (waiting_on_camera_data && waiting_on_mavlink_data) {
                // we are waiting for anything... this only occurs if we are not in sync
                // make a new AS_DATA element
                std::shared_ptr<AS_Data> sp(std::make_shared<AS_Data>()); // single memory allocation
                sp->cam = data;
                sp->modified = true;
                associated_data_entries.push_back(sp);
                waiting_on_camera_data = false;
                state = SYNCING;

            } else if (!waiting_on_camera_data && !waiting_on_mavlink_data){
                // not waiting on anything... make a new data entry
                std::shared_ptr<AS_Data> sp(std::make_shared<AS_Data>()); // single memory allocation
                sp->cam = data;
                sp->modified = true;
                associated_data_entries.push_back(sp);

                waiting_on_mavlink_data = true;

            } else if ( !waiting_on_mavlink_data && waiting_on_camera_data) {
                // we were not expecting mavlink data to arrive
                // we are out of sync
                state = NOT_SYNCED;

            } else {
                // we should never get here...
                // TODO handle this case if we get here somehow
            }
            break;
        }
        } // end data type switch

        invoke_consumer_callbacks();

        switch (state) {
        case NOT_SYNCED: {
            // attempt to sync
            associated_data_entries.clear(); // delete all entries in the vector
            waiting_on_mavlink_data = true;
            waiting_on_camera_data = true;

            break;
        }
        case SYNCING: {
            // syncing has started
            break;
        }
        case IN_SYNC: {
            // data sources are in sync, normal operation
            break;
        }
        } // end state machine switch
    }
}

// Invoke the functions that have registered with this associator
void Associator::invoke_consumer_callbacks(void) {
    for(auto sp : associated_data_entries) {
        for(auto& fun : _consumer_callbacks) {
            // pass the shared pointer to the consumers by value
            // the consumers determine when the AS_Data object will be cleaned up
            fun(sp);
        }
    }
}

//void create_new_container(void) {
//    std::shared_ptr<AS_Data> sp(std::make_shared<AS_Data>()); // single memory allocation
//    associated_data_entries.push_back(sp);
//}

void Associator::run(void) {
    while (!_stop) {
        process_incoming();
    }
}

void Associator::start(void) {
    _thread = std::thread(&Associator::run, this);
}

void Associator::stop(void) {
    _stop = true;
    _thread.join();
}

