#include <camera.h>
#include <includes.h>
#include <mutex>

#define MAVLINK_SYSTEM_ID 67
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


void handle_mavlink_msg(mavlink_message_t *msg)
{
	switch (msg->msgid)
	{
		case MAVLINK_MSG_ID_HEARTBEAT:
		{
			break;
		}

		case MAVLINK_MSG_ID_LOCAL_POSITION_NED:
		{
			break;
		}

		case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
		{
			break;
		}

		case MAVLINK_MSG_ID_ATTITUDE:
		{
			break;
		}

		case MAVLINK_MSG_ID_TIMESYNC:
		{
			// if we are receiving a timesync message we expect the tc1 field to be non-zero
//			std::cout << "got time sync msg: "<< &msg->ts0 << " : " << &msg->tc1 << std::endl;
			break;
		}

		case MAVLINK_MSG_ID_CAMERA_FEEDBACK:
		{
			// we have triggered an image capture and the AP has sent this msg to let us know it saw the exposure pulse
			// a corresponding message will be logged to the AP datalog
//			std::cout << "got camera feedback msg: "<< &msg->time_usec << " : " << &msg->img_idx << std::endl;
			break;
		}

		default:
		{
			break;
		}

	} // end: switch msgid
}

uint64_t get_time_usec()
{
	static struct timeval _time_stamp;
	gettimeofday(&_time_stamp, NULL);
	return _time_stamp.tv_sec*1000000 + _time_stamp.tv_usec;
}

/*
  write some data to the flight controller
 */
int mavlink_fc_write(const uint8_t *buf, size_t size)
{
	int bytesWritten = 0;
    if (serial_port_fd != -1) {
    	fc_mutex.lock();
    	bytesWritten = write(serial_port_fd, buf, size);
    	fc_mutex.unlock();
    }
    if (fc_udp_in_fd != -1) {
        if (fc_addrlen != 0) {
        	fc_mutex.lock();
        	bytesWritten = sendto(fc_udp_in_fd, buf, size, 0, (struct sockaddr*)&fc_addr, fc_addrlen);
        	fc_mutex.unlock();
        }
    }

    return bytesWritten;
}

/*
  send a mavlink message to flight controller
 */
int mavlink_fc_send(mavlink_message_t *msg)
{
    if (serial_port_fd != -1 || fc_udp_in_fd != -1) {
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];
        uint16_t len = mavlink_msg_to_send_buffer(buf, msg);
        return mavlink_fc_write(buf, len);
    }

    return -1; // failed to send the message
}

void companion_set_debug(int level)
{
    debug_level = level;
}

void companion_debug(int level)
{
    if (level > debug_level) {
        return;
    }
    // TODO run debug actions here...
}

/*
  open a UDP socket for taking messages from the flight controller
 */
static int udp_in_open(int port)
{
    struct sockaddr_in sock;
    int res;
    int one=1;

    memset(&sock,0,sizeof(sock));

    sock.sin_port = htons(port);
    /* sock.sin_addr.s_addr = htonl(INADDR_LOOPBACK); */
    sock.sin_family = AF_INET;

    res = socket(AF_INET, SOCK_DGRAM, 17);
    if (res == -1) {
        return -1;
    }

    setsockopt(res,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof(one));

    if (bind(res, (struct sockaddr *)&sock, sizeof(sock)) < 0) {
        return(-1);
    }

    return res;
}

/*
  open MAVLink serial port
 */
static int mavlink_serial_open(const char *path, unsigned baudrate)
{
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
    t.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE);
    t.c_cc[VMIN] = 0;
    t.c_cflag &= ~CRTSCTS;

    tcsetattr(fd, TCSANOW, &t);

    return fd;
}


/*
  main select loop
 */
static void select_loop()
{
	while (1) {
		fd_set fds;
		struct timeval tv;
		int numfd = 0;

		FD_ZERO(&fds);
		if (serial_port_fd != -1) {
			FD_SET(serial_port_fd, &fds);
			if (serial_port_fd >= numfd) {
				numfd = serial_port_fd+1;
			}
		}

		if (fc_udp_in_fd != -1) {
			FD_SET(fc_udp_in_fd, &fds);
			if (fc_udp_in_fd >= numfd) {
				numfd = fc_udp_in_fd+1;
			}
		}

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		int res = select(numfd, &fds, NULL, NULL, &tv);
		if (res <= 0) {
			continue;
		}

		if (fc_udp_in_fd != -1) {
			if (FD_ISSET(fc_udp_in_fd, &fds)) {
				// we have data pending
				uint8_t buf[3000];
				fc_addrlen = sizeof(fc_addr);
				fc_mutex.lock();
				ssize_t nread = recvfrom(fc_udp_in_fd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&fc_addr, &fc_addrlen);
				fc_mutex.unlock();
				if (nread <= 0) {
					/* printf("Read error from flight controller\n"); */
				} else {
					mavlink_message_t msg;
					mavlink_status_t status;
					for (uint16_t i=0; i<nread; i++) {
						if (mavlink_parse_char(MAVLINK_COMM_FC, buf[i], &msg, &status)) {
							handle_mavlink_msg(&msg);
						}
					}
				}
			}
		}

		// check for incoming bytes from flight controller
		if (serial_port_fd != -1) {
			if (FD_ISSET(serial_port_fd, &fds)) {
				uint8_t buf[MAVLINK_MAX_PACKET_LEN];
				fc_mutex.lock();
				ssize_t nread = read(serial_port_fd, buf, sizeof(buf));
				fc_mutex.unlock();
				if (nread <= 0) {
					printf("Read error from flight controller\n");
					// we should re-open serial port
					exit(1);
				}
				mavlink_message_t msg;
				mavlink_status_t status;
				for (uint16_t i=0; i<nread; i++) {
					if (mavlink_parse_char(MAVLINK_COMM_FC, buf[i], &msg, &status)) {
						handle_mavlink_msg(&msg);
					}
				}
			}
		}
//		mavlink_message_t msg;
//		int len = 0;
//		mavlink_param_request_list_t req_list;
//		req_list.target_component = 0;
//		req_list.target_system = 1;
//		mavlink_msg_param_request_list_encode(MAVLINK_SYSTEM_ID,
//				MAVLINK_COMPONENT_ID_REMOTE_LOG,
//											  &msg, &req_list);
//		len = mavlink_fc_send(&msg);
//		std::cout << "p!!! " << len << std::endl;
//
//		mavlink_heartbeat_t heart;
//		heart.type = MAV_TYPE_ONBOARD_CONTROLLER;
//		heart.autopilot = MAV_AUTOPILOT_INVALID;
//		heart.base_mode = MAV_MODE_FLAG_AUTO_ENABLED;
//		heart.system_status = MAV_STATE_ACTIVE;
//		mavlink_msg_heartbeat_encode(MAVLINK_SYSTEM_ID,
//											  MAVLINK_COMPONENT_ID_REMOTE_LOG,
//											  &msg, &heart);
//		len = mavlink_fc_send(&msg);
//		std::cout << "h!!! " << len << std::endl;
//		std::cout << get_time_usec() <<std::endl;
	}
}


int main(int argc, char *argv[])
{
	extern char *optarg;
	int opt;
	const char *serial_port = NULL;
	unsigned baudrate = 57600;
	const char *usage = "Usage: companion -b baudrate -s serial_port -d debug_level -f fc_udp_in";
	int fc_udp_in_port = -1;

	 while ((opt=getopt(argc, argv, "s:b:hd:f:")) != -1) {
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


