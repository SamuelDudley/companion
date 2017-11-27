#include <attitude.hpp>

using namespace moodycamel; // for lockless multi thread queue


void Attitude::start(void) {
    _thread = std::thread(&Attitude::run, this);
}

void Attitude::stop(void) {
    _stop = true;
    _thread.join();
}

void Attitude::run(void) {
    if (fc_udp_in_port != -1) {
        fc_udp_in_fd = udp_in_open(fc_udp_in_port);
        if (fc_udp_in_fd == -1) {
            std::cout << "Failed to open UDP-in socket\n" << std::endl;
            std::cout << "Producer will now exit\n" << std::endl;
            _stop = true;
        }
    } else {
        std::cout << "No UDP-in socket specified, MAVLink will not be received\n" << std::endl;
    }

    while (!_stop) {
        fd_set fds;
        struct timeval tv;
        int numfd = 0;

        FD_ZERO(&fds);

        if (fc_udp_in_fd != -1) {
            FD_SET(fc_udp_in_fd, &fds);
            if (fc_udp_in_fd >= numfd) {
                numfd = fc_udp_in_fd + 1;
            }
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int res = select(numfd, &fds, NULL, NULL, &tv);
        if (res <= 0) {
            continue;
        }

        if (fc_udp_in_fd != -1 && FD_ISSET(fc_udp_in_fd, &fds)) {
            // we have data pending
            uint8_t buf[3000];
            fc_addrlen = sizeof(fc_addr);
            fc_mutex.lock();
            ssize_t nread = recvfrom(fc_udp_in_fd, buf, sizeof(buf),
                    MSG_DONTWAIT, (struct sockaddr*) &fc_addr, &fc_addrlen);
            fc_mutex.unlock();
            if (nread > 0) {
                mavlink_message_t msg;
                mavlink_status_t status;
                for (uint16_t i = 0; i < nread; i++) {
                    if (mavlink_parse_char(MAVLINK_COMM_FC, buf[i], &msg, &status)) {
                        current_time = get_time_usec();
                        // these bytes contain a msg
                        handle_mavlink_msg(&msg);
                    }
                }
            }
        }
    }
}

void Attitude::handle_mavlink_msg(mavlink_message_t *msg) {
    switch (msg->msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: {
            mavlink_msg_heartbeat_decode(msg, &heartbeat); // decode the message into the strut
            std::cout << "got heartbeat in attitude " << std::endl;
            std::shared_ptr<Data> sp(std::make_shared<Attitude_Data>(heartbeat, current_time));
            bool succeeded = _cc_associator->in_data_queue.try_enqueue(std::move(sp));
            if (!succeeded) {
                std::cout << "image data not on queue!" << std::endl;
               // the queue is full, either increase depth, increase rate on other end or drop the data
               // TODO handle failure
            }
            _message_counter++;
            break;
        }
        default: {
            break;
        }
    }
    current_time = 0;
}

/*
 open a UDP socket for taking messages from the flight controller
 */
int Attitude::udp_in_open(int port) {
    struct sockaddr_in sock;
    int res;
    int one = 1;

    memset(&sock, 0, sizeof(sock));

    sock.sin_port = htons(port);
    /* sock.sin_addr.s_addr = htonl(INADDR_LOOPBACK); */
    sock.sin_family = AF_INET;

    res = socket(AF_INET, SOCK_DGRAM, 17);
    if (res == -1) {
        return -1;
    }

    setsockopt(res, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one));

    if (bind(res, (struct sockaddr *) &sock, sizeof(sock)) < 0) {
        return (-1);
    }

    return res;
}
