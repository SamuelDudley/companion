#include <camera.h>

void camera_loop()
{
	// image buffer
	XI_IMG image;
	memset(&image,0,sizeof(image));
	image.size = sizeof(XI_IMG);

	// Sample for XIMEA API V4.05
	HANDLE xiH = NULL;
	XI_RETURN stat = XI_OK;

	// Retrieving a handle to the camera device
	printf("Opening first camera...\n");
	stat = xiOpenDevice(0, &xiH);
	HandleResult(stat,"xiOpenDevice");

	// Setting "exposure" parameter (10ms=10000us)
	stat = xiSetParamInt(xiH, XI_PRM_EXPOSURE, 10000);
	HandleResult(stat,"xiSetParam (exposure set)");

	// set acquisition to frame rate mode
	xiSetParamInt(xiH,XI_PRM_ACQ_TIMING_MODE, XI_ACQ_TIMING_MODE_FRAME_RATE);
	// set frame rate
	xiSetParamInt(xiH,XI_PRM_FRAMERATE,20);
	int width_inc;
	xiGetParamInt(xiH, XI_PRM_HEIGHT XI_PRM_INFO_INCREMENT, &width_inc);
	std::cout << width_inc << std::endl;

//	xiSetParamInt(xiH, XI_PRM_WIDTH, 1660);
//	xiSetParamInt(xiH, XI_PRM_HEIGHT, 1660);
//	xiSetParamInt(xiH, XI_PRM_OFFSET_Y, 218);
//	xiSetParamInt(xiH, XI_PRM_OFFSET_X, 218);

	// https://www.ximea.com/support/wiki/apis/XiAPI_Manual#XI_PRM_TRANSPORT_DATA_TARGET-or-transport_data_target
	xiSetParamInt(xiH, XI_PRM_RECENT_FRAME, 1); // get most recent image from buffer

	// Note:
	// The default parameters of each camera might be different in different API versions
	// In order to ensure that your application will have camera in expected state,
	// please set all parameters expected by your application to required value.

	printf("Starting acquisition...\n");
	stat = xiStartAcquisition(xiH);
	HandleResult(stat,"xiStartAcquisition");


	#define EXPECTED_IMAGES 10
	for (int images=0;images < EXPECTED_IMAGES;images++)
	{


		// getting image from camera
		stat = xiGetImage(xiH, 5000, &image);
		HandleResult(stat,"xiGetImage");
		cv::Mat mat = cv::Mat(image.height, image.width, CV_8UC1, image.bp);
		cv::imwrite( "/tmp/images/"+std::to_string(images)+".jpg", mat );
		cv::namedWindow( "corners1", 1 );
		cv::imshow( "corners1", mat );
		cv::waitKey();

		printf("Image %d (%dx%d) received from camera.\n", images, (int)image.width, (int)image.height);
	}

	printf("Stopping acquisition...\n");
	xiStopAcquisition(xiH);
	xiCloseDevice(xiH);

	return;
}
