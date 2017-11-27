#pragma once
#include <iostream> // std::cout

#include <common.hpp>
#include <cameradata.hpp>
#include <attitudedata.hpp>

class AS_Data : public Data {
    /*
     * this needs to contain the following:
     * the data that has been associated
     * the consumers that have subscribed to this data
     * the consumers that have been notified of this data update
     */
public:
    AS_Data() :
        Data(DT_ASSOCIATED)
    {
       complete = false;
       modified = false;
       std::cout << "constructed DATA!" << std::endl;
    }

    ~AS_Data() {
        std::cout << "cleaned up DATA!" << std::endl;
    }

    bool complete; // set when the associator has all entries for this data set
    std::shared_ptr<Attitude_Data> msg;
    std::shared_ptr<Camera_Data> cam;
    bool modified;
    // subscribers (weak ptr? of consumer objects?)
};
