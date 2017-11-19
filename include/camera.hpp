#include <iostream>
#include <thread>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <m3api/xiApi.h>

void camera_loop();

#define HandleResult(res,place) if (res!=XI_OK) {printf("Error after %s (%d)\n",place,res);return;}
