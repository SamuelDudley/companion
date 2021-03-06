#include <camera.hpp>

	/*
	<message id="12007" name="MACHINE_VISION_CAPTURE_CONTROL">
		<description>Control on-board camera system</description>
		<field type="uint8_t" name="target_system">System ID</field>
		<field type="uint8_t" name="target_component">Component ID</field>
		<field type="uint8_t" name="session">0: stop, 1: start or keep it up </field>
		<field type="uint8_t" name="exposure_mode">0: manual, 1: auto</field>
		<field type="float" name="target_framerate">target framerate in Hz</field>
		<field type="uint8_t" name="command_id">Command Identity (incremental loop: 0 to 255)//A command sent multiple times will be executed or pooled just once</field>
    </message>
	<message id="12008" name="MACHINE_VISION_CAPTURE_STATUS">
		<description>Information about the status of a camera</description>
		<field type="uint8_t" name="target_system">System ID</field>
		<field type="uint8_t" name="target_component">Component ID</field>
		<field type="uint64_t" name="time_stamp" units="us">Timestamp (us)</field>
		<field type="uint16_t" name="frame_count">Current frame number in processing sequence</field>
		<field type="uint16_t" name="feedback_count">Current frame number in feedback (mavlink) sequence</field>
		<field type="uint8_t" name="image_status">Current status of image capturing (0: not running, 1: interval capture in progress, 2: error)</field>
		<field type="float" name="image_interval" units="Hz">Image capture interval in Hz</field>
		<field type="float" name="available_capacity" units="Mibytes">Available storage capacity in MiB</field>
	</message>

 	*/


void camera_loop()
{
	/* This is launched as a thread
	 * the camera is setup, and then we go into a capture and write to disk loop
	 * if this is being used for real the image buffer will need to be copied
	 * prior to be despatched for processing
	 */


	int image_counter=0;

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
	xiSetParamInt(xiH,XI_PRM_FRAMERATE,60);
	int width_inc;
	xiGetParamInt(xiH, XI_PRM_HEIGHT XI_PRM_INFO_INCREMENT, &width_inc);
	std::cout << width_inc << std::endl;

//	xiSetParamInt(xiH, XI_PRM_WIDTH, 1660);
//	xiSetParamInt(xiH, XI_PRM_HEIGHT, 1660);
//	xiSetParamInt(xiH, XI_PRM_OFFSET_Y, 218);
//	xiSetParamInt(xiH, XI_PRM_OFFSET_X, 218);

	// https://www.ximea.com/support/wiki/apis/XiAPI_Manual#XI_PRM_TRANSPORT_DATA_TARGET-or-transport_data_target
//	xiSetParamInt(xiH, XI_PRM_RECENT_FRAME, 1); // get most recent image from buffer

	// Note:
	// The default parameters of each camera might be different in different API versions
	// In order to ensure that your application will have camera in expected state,
	// please set all parameters expected by your application to required value.

	printf("Starting acquisition...\n");
	stat = xiStartAcquisition(xiH);
	HandleResult(stat,"xiStartAcquisition");

	// image buffer
	XI_IMG image;
	memset(&image,0,sizeof(image));
	image.size = sizeof(XI_IMG);

	while (1) {
		// getting image from camera
		stat = xiGetImage(xiH, 5000, &image);
		HandleResult(stat,"xiGetImage");
		cv::Mat mat = cv::Mat(image.height, image.width, CV_8UC1, image.bp);
		cv::imwrite( "/tmp/images/"+std::to_string(image_counter)+".jpg", mat);
//		cv::namedWindow( "corners1", 1 );
//		cv::imshow( "corners1", mat );
//		cv::waitKey();
		cv::Mat B = mat.clone(); // deep copy of mat image... we can safely send this off to be processed
		image_counter++;

		printf("Image %d (%dx%d) received from camera.\n", image_counter, (int)image.width, (int)image.height);
	}

    // TODO: allow this thread to restart the camera and change settings

	printf("Stopping acquisition...\n");
	xiStopAcquisition(xiH);
	xiCloseDevice(xiH);

	return;


}
