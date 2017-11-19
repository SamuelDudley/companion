#include <camera.hpp>
#include <includes.hpp>
#include <unistd.h> // for creating symlinks and unlinking old ones
#include <mutex>
#include <fstream>
#include <chrono>
#include <ctime>

#define MAVLINK_SYSTEM_ID 122
#define MAVLINK_TARGET_SYSTEM_ID 1
#define MAVLINK_COMM_NUM_BUFFERS 1
#define MAVLINK_COMPONENT_ID_REMOTE_LOG 72

#include <mavlink/ardupilotmega/mavlink.h>
#include <mavlink/mavlink_helpers.h>

static int serial_port_fd = -1;
static int fc_udp_in_fd = -1;
static int debug_level;

std::mutex fc_mutex;

struct sockaddr_in fc_addr;
socklen_t fc_addrlen;

# define MAVLINK_COMM_FC MAVLINK_COMM_0

mavlink_camera_feedback_t camera_feedback;
mavlink_heartbeat_t heartbeat;

void handle_mavlink_msg(mavlink_message_t *msg) {
    switch (msg->msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT: {
        mavlink_msg_heartbeat_decode(msg, &heartbeat); // decode the message into the strut
        std::cout << "got heartbeat " << std::endl;
        break;
    }

    case MAVLINK_MSG_ID_LOCAL_POSITION_NED: {
        break;
    }

    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
        break;
    }

    case MAVLINK_MSG_ID_ATTITUDE: {
        break;
    }

    case MAVLINK_MSG_ID_TIMESYNC: {
        // if we are receiving a timesync message we expect the tc1 field to be non-zero
//			std::cout << "got time sync msg: "<< &msg->ts0 << " : " << &msg->tc1 << std::endl;
        break;
    }

    case MAVLINK_MSG_ID_CAMERA_FEEDBACK: {
        // we have triggered an image capture and the AP has sent this msg to let us know it saw the exposure pulse
        // a corresponding message will be logged to the AP datalog

        mavlink_msg_camera_feedback_decode(msg, &camera_feedback); // decode the message into the strut
        std::cout << "got camera feedback msg: " << camera_feedback.time_usec
                << " : " << camera_feedback.img_idx << std::endl;
        break;
    }

    case MAVLINK_MSG_ID_MACHINE_VISION_FEEDBACK: {
        std::cout << "got MACHINE_VISION_FEEDBACK msg" << std::endl;
        /*
         <message id="12009" name="MACHINE_VISION_FEEDBACK">
         <description>Report frame capture of machine vision camera</description>
         <field type="uint8_t" name="target_system">System ID of companion computer</field>
         <field type="uint8_t" name="target_component">Component ID</field>
         <field type="uint64_t" name="ap_time_stamp">Autopilot frame capture time (us)</field>
         <field type="uint64_t" name="ekf_time_stamp">EKF sample timestamp (us)</field>
         <field type="uint16_t" name="frame_count">Current frame number in processing sequence</field>
         <field type="float" name="roll" units="rad">Roll angle (rad, -pi..+pi)</field>
         <field type="float" name="pitch" units="rad">Pitch angle (rad, -pi..+pi)</field>
         <field type="float" name="yaw" units="rad">Yaw angle (rad, -pi..+pi)</field>
         <field type="float" name="q1">Quaternion component 1, w (1 in null-rotation)</field>
         <field type="float" name="q2">Quaternion component 2, x (0 in null-rotation)</field>
         <field type="float" name="q3">Quaternion component 3, y (0 in null-rotation)</field>
         <field type="float" name="q4">Quaternion component 4, z (0 in null-rotation)</field>
         <field type="float" name="vel_x">velocity North (m/s)</field>
         <field type="float" name="vel_y">velocity East (m/s)</field>
         <field type="float" name="vel_z">velocity Down (m/s)</field>
         <field type="float" name="pos_x">position North (m)</field>
         <field type="float" name="pos_y">position East (m)</field>
         <field type="float" name="pos_z">position Down (m)</field>
         </message>
         */
        break;
    }

    default: {
        break;
    }

    } // end: switch msgid
}

uint64_t get_time_usec() {
    static struct timeval _time_stamp;
    gettimeofday(&_time_stamp, NULL);
    return _time_stamp.tv_sec * 1000000 + _time_stamp.tv_usec;
}

std::string get_date_string() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    char timebuff[120] = { 0 };
    std::strftime(timebuff, 120, "%d%m%y_%H%M%S", &tm);
    return std::string(timebuff);
}

/*
 write some data to the flight controller
 */
int mavlink_fc_write(const uint8_t *buf, size_t size) {
    int bytesWritten = 0;
    if (serial_port_fd != -1) {
        fc_mutex.lock();
        bytesWritten = write(serial_port_fd, buf, size);
        fc_mutex.unlock();
    }
    if (fc_udp_in_fd != -1) {
        if (fc_addrlen != 0) {
            fc_mutex.lock();
            bytesWritten = sendto(fc_udp_in_fd, buf, size, 0,
                    (struct sockaddr*) &fc_addr, fc_addrlen);
            fc_mutex.unlock();
        }
    }

    return bytesWritten;
}

/*
 send a mavlink message to flight controller
 */
int mavlink_fc_send(mavlink_message_t *msg) {
    if (serial_port_fd != -1 || fc_udp_in_fd != -1) {
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];
        uint16_t len = mavlink_msg_to_send_buffer(buf, msg);
        return mavlink_fc_write(buf, len);
    }

    return -1; // failed to send the message
}

void companion_set_debug(int level) {
    debug_level = level;
}

void companion_debug(int level) {
    if (level > debug_level) {
        return;
    }
    // TODO run debug actions here...
}

/*
 open a UDP socket for taking messages from the flight controller
 */
static int udp_in_open(int port) {
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

/*
 open MAVLink serial port
 */
static int mavlink_serial_open(const char *path, unsigned baudrate) {
    int fd = open(path, O_RDWR);
    if (fd == -1) {
        perror(path);
        return -1;
    }

    struct termios t;
    memset(&t, 0, sizeof(t));

    tcgetattr(fd, &t);

    cfsetspeed(&t, baudrate);

    t.c_iflag &= ~(BRKINT | ICRNL | IMAXBEL | IXON | IXOFF);
    t.c_oflag &= ~(OPOST | ONLCR);
    t.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHOCTL
            | ECHOKE);
    t.c_cc[VMIN] = 0;
    t.c_cflag &= ~CRTSCTS;

    tcsetattr(fd, TCSANOW, &t);

    return fd;
}

/*
 main select loop
 */
static void select_loop() {
    std::string raw_telem_filename = get_date_string() + ".tlog";
    std::string last_log_filename = "last_log.tlog";
    std::cout << raw_telem_filename << std::endl;
    std::ofstream mav_file(raw_telem_filename,
            std::ofstream::out | std::ofstream::binary);

    unlink(last_log_filename.c_str()); // unlink a symlink if it exists
    symlink(raw_telem_filename.c_str(), last_log_filename.c_str()); // create a new symlink to the latest log file

    while (1) {
        fd_set fds;
        struct timeval tv;
        int numfd = 0;

        FD_ZERO(&fds);
        if (serial_port_fd != -1) {
            FD_SET(serial_port_fd, &fds);
            if (serial_port_fd >= numfd) {
                numfd = serial_port_fd + 1;
            }
        }

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
                uint64_t current_time;
                mavlink_message_t msg;
                mavlink_status_t status;
                for (uint16_t i = 0; i < nread; i++) {
//						mav_file << buf[i]; //this is the raw byte stream
                    if (mavlink_parse_char(MAVLINK_COMM_FC, buf[i], &msg,
                            &status)) {
                        current_time = get_time_usec();
                        mav_file.write(
                                reinterpret_cast<const char *>(&current_time),
                                sizeof(current_time));
                        // these bytes contain a msg
                        mav_file.write((char*) buf, i + 1);
                        handle_mavlink_msg(&msg);
                        mav_file.flush();
                    }
                }
            }
        }

        // check for incoming bytes from flight controller
        if (serial_port_fd != -1 && FD_ISSET(serial_port_fd, &fds)) {
            uint8_t buf[MAVLINK_MAX_PACKET_LEN];
            fc_mutex.lock();
            ssize_t nread = read(serial_port_fd, buf, sizeof(buf));
            fc_mutex.unlock();
            if (nread <= 0) {
                printf("Read error from flight controller\n");
                // we should re-open serial port
                exit(1);
            }
            uint64_t current_time;
            mavlink_message_t msg;
            mavlink_status_t status;
            for (uint16_t i = 0; i < nread; i++) {
//					mav_file << buf[i]; //this is the raw byte stream
                if (mavlink_parse_char(MAVLINK_COMM_FC, buf[i], &msg,
                        &status)) {
                    std::cout << current_time << std::endl;
                    mav_file.write(
                            reinterpret_cast<const char *>(&current_time),
                            sizeof(current_time));
                    // these bytes contain a msg
                    mav_file.write((char*) buf, i + 1);
                    handle_mavlink_msg(&msg);
                    mav_file.flush();
                }
            }
        }
    } // main while(1) loop
      // we only get here is an exit is requested
    mav_file.close();
}

int main(int argc, char *argv[]) {
    extern char *optarg;
    int opt;
    const char *serial_port = NULL;
    unsigned baudrate = 115200;
    const char *usage =
            "Usage: companion -b baudrate -s serial_port -d debug_level -f fc_udp_in";
    int fc_udp_in_port = -1;

    while ((opt = getopt(argc, argv, "s:b:hd:f:")) != -1) {
        switch (opt) {
        case 's':
            serial_port = optarg;
            break;
        case 'b':
            baudrate = atoi(optarg);
            break;
        case 'd':
            companion_set_debug(atoi(optarg));
            break;
        case 'f':
            fc_udp_in_port = atoi(optarg);
            break;
        case 'h':
        default:
            printf("%s\n", usage);
            exit(1);
            break;
        }
    }

    if (!serial_port && fc_udp_in_port == -1) {
        printf("%s\n", usage);
        exit(1);
    }

    if (serial_port) {
        serial_port_fd = mavlink_serial_open(serial_port, baudrate);
        if (serial_port_fd == -1) {
            printf("Failed to open mavlink serial port %s\n", serial_port);
            exit(1);
        }
    }

    if (fc_udp_in_port != -1) {
        fc_udp_in_fd = udp_in_open(fc_udp_in_port);
        if (fc_udp_in_fd == -1) {
            printf("Failed to open UDP-in socket\n");
            exit(1);
        }
    }

//    std::thread camera_thread(camera_loop);
//    camera_thread.detach();
    select_loop();
}

