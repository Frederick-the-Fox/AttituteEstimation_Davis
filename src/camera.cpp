#include "../include/camera.hpp"

using namespace std;

float getDistance(CvPoint pointO, CvPoint pointA)
{
	float distance;
	distance = powf((pointO.x - pointA.x), 2) + powf((pointO.y - pointA.y), 2);
	distance = sqrtf(distance);
	return distance;
}

camera::camera()
{
	std::cout << "inited" << std::endl;
}

camera::~camera()
{
	std::cout << "deleted" << std::endl;
}

void camera::function() {

	vector<cv::Point> points_selected(3);
	// static atomic_bool globalShutdown(false);

	// static void globalShutdownSignalHandler(int signal) {
	// 	// Simply set the running flag to false on SIGTERM and SIGINT (CTRL+C) for global shutdown.
	// 	if (signal == SIGTERM || signal == SIGINT) {
	// 		globalShutdown.store(true);
	// 	}
	// }

	// static void usbShutdownHandler(void *ptr) {
	// 	(void) (ptr); // UNUSED.

	// 	globalShutdown.store(true);
	// }
	// Install signal handler for global shutdown.
	// struct sigaction shutdownAction;

	// shutdownAction.sa_handler = &globalShutdownSignalHandler;
	// shutdownAction.sa_flags   = 0;
	// sigemptyset(&shutdownAction.sa_mask);
	// sigaddset(&shutdownAction.sa_mask, SIGTERM);
	// sigaddset(&shutdownAction.sa_mask, SIGINT);

	// if (sigaction(SIGTERM, &shutdownAction, NULL) == -1) {
	// 	libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
	// 		"Failed to set signal handler for SIGTERM. Error: %d.", errno);
	// 	return (EXIT_FAILURE);
	// }

	// if (sigaction(SIGINT, &shutdownAction, NULL) == -1) {
	// 	libcaer::log::log(libcaer::log::logLevel::CRITICAL, "ShutdownAction",
	// 		"Failed to set signal handler for SIGINT. Error: %d.", errno);
	// 	return (EXIT_FAILURE);
	// }

	// Open a DAVIS, give it a device ID of 1, and don't care about USB bus or SN restrictions.
	libcaer::devices::davis davisHandle = libcaer::devices::davis(1);

	// Let's take a look at the information we have on the device.
	struct caer_davis_info davis_info = davisHandle.infoGet();

	// printf("%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n", davis_info.deviceString,
	// 	davis_info.deviceID, davis_info.deviceIsMaster, davis_info.dvsSizeX, davis_info.dvsSizeY,
	// 	davis_info.logicVersion);

	// Send the default configuration before using the device.
	// No configuration is sent automatically!
	davisHandle.sendDefaultConfig();

	// Tweak some biases, to increase bandwidth in this case. 
	struct caer_bias_coarsefine coarseFineBias;//定义一个畸变，具体各个值的含义可以去头文件看

	coarseFineBias.coarseValue        = 2; 
	coarseFineBias.fineValue          = 116;
	coarseFineBias.enabled            = true;
	coarseFineBias.sexN               = false;
	coarseFineBias.typeNormal         = true;
	coarseFineBias.currentLevelNormal = true;

	davisHandle.configSet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP, caerBiasCoarseFineGenerate(coarseFineBias));

	coarseFineBias.coarseValue        = 1;
	coarseFineBias.fineValue          = 33;
	coarseFineBias.enabled            = true;
	coarseFineBias.sexN               = false;
	coarseFineBias.typeNormal         = true;
	coarseFineBias.currentLevelNormal = true;

	davisHandle.configSet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP, caerBiasCoarseFineGenerate(coarseFineBias));

	// Let's verify they really changed!
	// uint32_t prBias   = davisHandle.configGet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP);
	// uint32_t prsfBias = davisHandle.configGet(DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP);

	// printf("New bias values --- PR-coarse: %d, PR-fine: %d, PRSF-coarse: %d, PRSF-fine: %d.\n",
	// 	caerBiasCoarseFineParse(prBias).coarseValue, caerBiasCoarseFineParse(prBias).fineValue,
	// 	caerBiasCoarseFineParse(prsfBias).coarseValue, caerBiasCoarseFineParse(prsfBias).fineValue);

	// Now let's get start getting some data from the device. We just loop in blocking mode,
	// no notification needed regarding new events. The shutdown notification, for example if
	// the device is disconnected, should be listened to.
	davisHandle.dataStart(nullptr, nullptr, nullptr, nullptr, nullptr);

	// Let's turn on blocking data-get mode to avoid wasting resources.
	davisHandle.configSet(CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true);

	davisHandle.configSet(DAVIS_CONFIG_APS, DAVIS_CONFIG_APS_GLOBAL_SHUTTER, false);
	davisHandle.configSet(DAVIS_CONFIG_APS, DAVIS_CONFIG_APS_AUTOEXPOSURE, false);
	davisHandle.configSet(DAVIS_CONFIG_APS, DAVIS_CONFIG_APS_EXPOSURE, 4200);

	cv::namedWindow("PLOT_EVENTS",
		cv::WindowFlags::WINDOW_AUTOSIZE | cv::WindowFlags::WINDOW_KEEPRATIO | cv::WindowFlags::WINDOW_GUI_EXPANDED);
	cv::namedWindow("PLOT_FRAME",
		cv::WindowFlags::WINDOW_AUTOSIZE | cv::WindowFlags::WINDOW_KEEPRATIO | cv::WindowFlags::WINDOW_GUI_EXPANDED);

	while (1) {
		std::unique_ptr<libcaer::events::EventPacketContainer> packetContainer = davisHandle.dataGet();
		if (packetContainer == nullptr) {
			continue; // Skip if nothing there.
		}

		// printf("\nGot event container with %d packets (allocated).\n", packetContainer->size());

		for (auto &packet : *packetContainer) {
			if (packet == nullptr) {
				// printf("Packet is empty (not present).\n");
				continue; // Skip if nothing there.
			}

			// printf("Packet of type %d -> %d events, %d capacity.\n", packet->getEventType(), packet->getEventNumber(),
				// packet->getEventCapacity());

			if (packet->getEventType() == POLARITY_EVENT) {
				std::shared_ptr<const libcaer::events::PolarityEventPacket> polarity
					= std::static_pointer_cast<libcaer::events::PolarityEventPacket>(packet);

				// Get full timestamp and addresses of first event.
				const libcaer::events::PolarityEvent &firstEvent = (*polarity)[0];

				int32_t ts = firstEvent.getTimestamp();
				uint16_t x = firstEvent.getX();
				uint16_t y = firstEvent.getY();
				bool pol   = firstEvent.getPolarity();

				// printf("First polarity event - ts: %d, x: %d, y: %d, pol: %d.\n", ts, x, y, pol);

				cv::Mat cvEvents(davis_info.dvsSizeY, davis_info.dvsSizeX, CV_8UC3, cv::Vec3b{127, 127, 127});
				for (const auto &e : *polarity) {
					cvEvents.at<cv::Vec3b>(e.getY(), e.getX())
						= e.getPolarity() ? cv::Vec3b{255, 255, 255} : cv::Vec3b{0, 0, 0};
				}

				//findingalgorithm:
				cv::Mat imgGray;
				cv::Mat imgBinary;
				vector<vector<cv::Point>> contours;
				vector<cv::Vec4i> hierachy;
				int vecnumber = 0;
				vector<int> i_of_contours;
				

				// cout << "width:" << cvEvents.cols << endl;
				// cout << "height:" << cvEvents.rows << endl;

				cv::Point2f centerP(120, 90);

				cv::cvtColor(cvEvents, imgGray, CV_BGR2GRAY);
				cv::threshold(imgGray, imgBinary, 100, 250, cv::THRESH_BINARY_INV);

				cv::Mat element_1 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, 1));
				cv::Mat element_2 = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));

				cv::erode(imgBinary, imgBinary, element_2);
				cv::dilate(imgBinary, imgBinary, element_1);

				cv::findContours(imgBinary, contours, hierachy, CV_RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

				map<int, int> m_i2d;
				map<int, cv::Point> m_i2p;

				for (int i = 0; i < contours.size(); i++)
				{
					cv::Point center;
					cv::RotatedRect rect=minAreaRect(contours[i]);
					cv::Point2f P[4];
					rect.points(P);
					int h = getDistance(P[0], P[1]);
					int w = getDistance(P[1], P[2]);
					int area = h * w;
					center.x = (P[0].x + P[2].x)/2;
					center.y = (P[0].y + P[2].y)/2;

					float distance = sqrtf(pow((center.x - centerP.x), 2) + pow((center.y - centerP.y), 2)); 

					m_i2d[i] = distance;
					m_i2p[i] = center;
						
					if (vecnumber <= 3)
					{
						i_of_contours.push_back(i);
						vecnumber += 1;
					}
					else
					{
						for (int j = 0; j < 3; j++)
						{
							if (m_i2d[i] < m_i2d[i_of_contours[j]])
							{
								i_of_contours[j] = i;
								break;
							}
						}
					}

				}

				if (i_of_contours.size() == 0)
				{
					cout << "未获取到事件帧" << endl;
				}
				else{
					for(int i = 0; i < i_of_contours.size(); i++)
					{
						// cv::circle(imgBinary, m_i2p[i_of_contours[i]], 1, cv::Scalar(0,0,255.0), 1, 1, 0);
						// cv::drawContours(imgBinary, contours[i_of_contours[i]], 0, cv::Scalar(255), 2);
						points_selected[i] = m_i2p[i_of_contours[i]];
						cout << "the current number of i_of_contours is:" << i << endl;
					}
				}

				// cv::circle(imgBinary, centerP, 5, cv::Scalar(0,0,255.0), 3, 8, 0);
				cv::imshow("PLOT_EVENTS", imgBinary);

				// map<vector<cv::Point>, cv::Point> m_c2p;
				
				cv::waitKey(1);
				
			}

			if (packet->getEventType() == FRAME_EVENT) {
				std::shared_ptr<const libcaer::events::FrameEventPacket> frame
					= std::static_pointer_cast<libcaer::events::FrameEventPacket>(packet);

				// Get full timestamp, and sum all pixels of first frame event.
				const libcaer::events::FrameEvent &firstEvent = (*frame)[0];

				int32_t ts   = firstEvent.getTimestamp();
				uint64_t sum = 0;

				for (int32_t y = 0; y < firstEvent.getLengthY(); y++) {
					for (int32_t x = 0; x < firstEvent.getLengthX(); x++) {
						sum += firstEvent.getPixel(x, y);
					}
				}

				// printf("First frame event - ts: %d, sum: %" PRIu64 ".\n", ts, sum);

				for (const auto &f : *frame) {
					if (f.getROIIdentifier() != 0) {
						continue;
					}

					cv::Mat cvFrame = f.getOpenCVMat(false);
					// Simple display, just use OpenCV GUI.
					cv::cvtColor(cvFrame, cvFrame, cv::COLOR_GRAY2BGR);
					
					for (int i = 0; i < points_selected.size(); i++)
					{
						cv::circle(cvFrame, points_selected[i], 3, cv::Scalar(0,0,255.0), 3, 3, 0);
						cout << "the current number of points_selected is:" << i << endl;
					}
					cv::imshow("PLOT_FRAME", cvFrame);
					cv::waitKey(1);
				}
			}

		}
	}

	davisHandle.dataStop();
	// Close automatically done by destructor.

	cv::destroyWindow("PLOT_EVENTS");
	cv::destroyWindow("PLOT_FRAME");

	// printf("Shutdown successful.\n");
}

