#pragma once

#include <thread>

#include <common.hpp>
#include <associate.hpp>

#include <attitudedata.hpp>
#include <includes.hpp>
#include <mavlink/mavlink_helpers.h>
#include <mavlink/ardupilotmega/mavlink.h>

# define MAVLINK_COMM_FC MAVLINK_COMM_0

class Attitude {
public:
    Attitude(std::shared_ptr<Associator> obj_associator, int idx, int port = -1) :
        _stop(false),
        _message_counter(0),
        fc_udp_in_fd(-1)
        {
            _cc_associator = obj_associator;
            _idx = idx;
            fc_udp_in_port = port;
            current_time = 0;
        }

    ~Attitude() {
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

protected:
    std::mutex fc_mutex;

    void run(void);
    static int udp_in_open(int port);
    void handle_mavlink_msg(mavlink_message_t *msg);

    mavlink_heartbeat_t heartbeat;

    std::shared_ptr<Associator> _cc_associator; // pointer to associator object
    int _idx;

    std::atomic_bool _stop;
    std::thread _thread;

    int _message_counter;

    int fc_udp_in_fd;
    int fc_udp_in_port;

    struct sockaddr_in fc_addr;
    socklen_t fc_addrlen;
    uint64_t current_time;

};
