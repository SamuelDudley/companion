#include <common.hpp>

uint64_t get_time_usec(void) {
    static struct timeval _time_stamp;
    gettimeofday(&_time_stamp, NULL);
    return _time_stamp.tv_sec * 1000000 + _time_stamp.tv_usec;
}

std::string get_date_string(void) {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    char timebuff[120] = { 0 };
    std::strftime(timebuff, 120, "%d%m%y_%H%M%S", &tm);
    return std::string(timebuff);
}
