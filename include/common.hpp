#pragma once

#include <ctime>
#include <sys/time.h>
#include <fstream>

#include <mavlink/ardupilotmega/mavlink.h>
#include <mavlink/mavlink_helpers.h>

std::string get_date_string(void);
uint64_t get_time_usec(void);


enum Data_Type : int {
  DT_ATTITUDE = 0,
  DT_CAMERA,
  DT_ASSOCIATED
};

// abstract data class used to pass data between producers, associator and consumers
class Data {
public:
    Data(int data_type) :
        _type(data_type)
    {
    }
    virtual ~Data(){}; // virtual destructor, deleting a Data instance will delete the derived class object too
    inline const int get_type(void) {return _type;}
protected:
    const int _type;
};

