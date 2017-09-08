// Copyright (C) 2017 Electronic Arts Inc.  All rights reserved.

#include "cameras.hpp"
#include "recorder.hpp"
#include "base64.hpp"
#include "color_correction.hpp"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <iostream>
#include <numeric>

#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include "boost/date_time/posix_time/posix_time.hpp"

Camera::Camera()
{
	m_unique_id = boost::uuids::to_string(boost::uuids::random_generator()());
	m_model = "unknown";
	m_effective_fps = 0.0f;
	m_recording = false;
	m_waiting_for_trigger = false;
	m_waiting_for_trigger_hold = false;
	m_capturing = false;
	m_framerate = 24;
	m_width = 320;
	m_height = 200;
	m_bitcount = 8;
	m_record_frames_remaining = -1;
	m_image_counter = 0;
	m_using_hardware_sync = false;
	m_display_focus_peak = false;
	m_display_overexposed = false;
	m_display_histogram = false;
	default_preview_res = 160;
	m_preview_width = default_preview_res;
	m_preview_height = default_preview_res;
	m_color_need_debayer = false;

	m_debug_in_capture_cycle = false;

	m_encoding_buffers_used = 0;
	m_writing_buffers_used = 0;
}

Camera::~Camera()
{

}

std::string Camera::toString() const
{
	std::stringstream ss;
	ss << "Camera " << m_model << "/" << m_unique_id << " " << m_width << "x" << m_height << " rate:" << m_framerate << "fps current:" << m_effective_fps << " C:" << (m_capturing ? "Y" : "N") << " R:" << (m_recording ? "Y" : "N") << " " << m_version;
	return ss.str();
}

void Camera::got_frame_timeout()
{
	m_effective_fps = 0.0f;
}

void Camera::got_image(cv::Mat img, double ts, int width, int height, int bitcount, int channels, int black_level)
{
	// Called by the camera implementation each time we recieve a new image

	//auto time1 = boost::posix_time::microsec_clock::local_time();

	if (channels != 1) {
		std::cerr << "TODO multiple channels not implemented in recorders" << std::endl;
		return;
	}

	assert(bitcount == 8 || bitcount == 16);
	assert(m_bitcount >= 8);
	assert(m_bitcount <= 16);

	// real representing this frame's time in seconds since the beginning of recording
	double frame_timestamp = ts > 0.0 ? ts : ((boost::posix_time::microsec_clock::local_time() - boost::posix_time::ptime(boost::gregorian::date(2016, 1, 1))).total_milliseconds() / 1000.0);
	if (m_image_counter == 0)
	{ 
		m_start_ts = frame_timestamp;
		m_last_ts = 0.0;
	}		
	frame_timestamp = frame_timestamp - m_start_ts;

	// Compute average effective framerate
	double dt = frame_timestamp - m_last_ts;
	m_last_ts = frame_timestamp;
	if (dt > 0.0)
	{
		ts_d.add(1.0 / dt);
		m_effective_fps = ts_d.average();
	}

	// Generate thumbnail for live preview
	if (!m_recording)
	{
		if (m_display_focus_peak)
		{
			cv::Mat last_image;
			cv::Mat work;

			if (m_color_need_debayer && img.channels() == 1)
				cv::resize(img, last_image, cv::Size(0, 0), 0.5, 0.5, cv::INTER_NEAREST); // Color Camera Bayer: skip debayer by resizing
			else if (img.channels() == 3)
				cv::cvtColor(img, last_image, cv::COLOR_BGR2GRAY); // Native Color Camera BGR
			else
				last_image = img.clone(); // Grayscale camera

			if (m_bitcount > 8) // convert the image to 8 bit range, the histogram expects values from 0 to 255
				last_image.convertTo(last_image, CV_8U, 1.0f / (1 << (m_bitcount - 8)));

			// Focus Peaking: display yellow overlay for areas with high contrast
			int focus_threshold = 80;
			cv::Mat kernel = cv::Mat::ones(3, 3, CV_8S);
			kernel.at<char>(1, 1) = -8;
			cv::filter2D(last_image, work, -1, kernel);
			cv::threshold(work, work, focus_threshold, 255, cv::THRESH_BINARY);
			kernel = cv::Mat::ones(5, 5, CV_8U);
			cv::dilate(work, work, kernel, cv::Point(-1, -1), 2);
			cv::Mat invert = cv::Scalar::all(255) - work;
			cv::Mat RG;
			cv::max(work, last_image, RG);
			cv::Mat B;
			cv::min(invert, last_image, B);
			cv::Mat arr[] = { B, RG, RG };

			cv::Mat new_preview_image;

			cv::merge(arr, 3, img);

			cv::resize(img, new_preview_image, cv::Size(m_preview_width, m_preview_height), 0.0, 0.0, cv::INTER_NEAREST);
			color_correction::linear_to_sRGB(new_preview_image);

			{
				std::lock_guard<std::mutex> lock(m_mutex_preview_image);
				preview_image = new_preview_image;
			}
		}
		else if (m_display_overexposed)
		{
			cv::Mat last_image;
			cv::Mat work;

			if (m_color_need_debayer && img.channels() == 1)
				cv::resize(img, last_image, cv::Size(0, 0), 0.5, 0.5, cv::INTER_NEAREST); // Color Camera Bayer: skip debayer by resizing
			else if (img.channels() == 3)
				cv::cvtColor(img, last_image, cv::COLOR_BGR2GRAY); // Native Color Camera BGR
			else
				last_image = img.clone(); // Grayscale camera

			if (m_bitcount > 8) // convert the image to 8 bit range, the histogram expects values from 0 to 255
				last_image.convertTo(last_image, CV_8U, 1.0f / (1 << (m_bitcount - 8)));

			// Overexposure check : display red overlay for overexposed areas
			int overexposed_threshold = 250;
			cv::threshold(last_image, work, overexposed_threshold, 255, cv::THRESH_BINARY);
			cv::Mat kernel = cv::Mat::ones(5, 5, CV_8U);
			cv::dilate(work, work, kernel, cv::Point(-1, -1), 2);
			cv::Mat R;
			cv::max(work, last_image, R);
			cv::Mat GB;
			cv::Mat invert = cv::Scalar::all(255) - work;
			cv::min(invert, last_image, GB);
			cv::Mat arr[] = { GB, GB, R };

			cv::Mat new_preview_image;

			cv::merge(arr, 3, img);
			cv::resize(img, new_preview_image, cv::Size(m_preview_width, m_preview_height), 0.0, 0.0, cv::INTER_NEAREST);
			color_correction::linear_to_sRGB(new_preview_image);

			{
				std::lock_guard<std::mutex> lock(m_mutex_preview_image);
				preview_image = new_preview_image;
			}
		}
		else if (m_display_histogram)
		{
			int histSize = 256;
			float range[] = { 0, 256 };
			const float* histRange = { range };
			bool uniform = true; bool accumulate = false;

			cv::Mat last_image;

			if (m_color_need_debayer && img.channels() == 1)
				cv::cvtColor(img, last_image, cv::COLOR_BayerBG2RGB);
			else
				last_image = img.clone(); // Grayscale camera

			if (m_bitcount > 8) // convert the image to 8 bit range, the histogram expects values from 0 to 255
				last_image.convertTo(last_image, CV_8U, 1.0f / (1 << (m_bitcount - 8)));

			int hist_w = histSize; 
			int hist_h = 256;
			cv::Mat histImage;

			{
				// Create Histogram

				cv::Mat sourceChannels[3];
				cv::Mat histChannels[3];

				cv::split(last_image, sourceChannels);

				for (int c = 0; c < last_image.channels(); c++)
				{
					histChannels[c] = cv::Mat(hist_h, hist_w, CV_8UC1, cv::Scalar(0));

					cv::Mat b_hist;
					cv::calcHist(&sourceChannels[c], 1, 0, cv::Mat(), b_hist, 1, &histSize, &histRange, uniform, accumulate);
					b_hist = b_hist * (32.0 * hist_h / (width*height));

					for (int i = 0; i < histSize; i++)
					{
						static const int extreme_width = 4;
						static const cv::Scalar highlight = cv::Scalar(255, 0, 0);
						static const cv::Scalar grey = cv::Scalar(200, 0, 0);
						line(histChannels[c],
							cv::Point(i, hist_h),
							cv::Point(i, hist_h - cvRound(b_hist.at<float>(i))),
							i <= extreme_width || i >= histSize - 1 - extreme_width ? highlight : grey);
					}
				}

				cv::merge(histChannels, last_image.channels(), histImage);
			}

			{
				std::lock_guard<std::mutex> lock(m_mutex_preview_image);
				preview_image = histImage;
			}
		}
		else
		{
			// No histogram, the image is raw from the camera, or overexposed/focuspeak
			// Store smaller resized preview image

			cv::Mat new_preview_image;
			cv::Mat tempImage = img;

			// Debayer only if this is the raw from camera
			if (m_color_need_debayer && img.channels()==1)
				cv::cvtColor(img, tempImage, cv::COLOR_BayerBG2RGB);

			cv::resize(tempImage, new_preview_image, cv::Size(m_preview_width, m_preview_height), 0.0, 0.0, cv::INTER_NEAREST);

			// Color-correct only if this is the raw from camera
			if (m_color_need_debayer)
				color_correction::apply(new_preview_image, m_color_balance, black_level);

			if (m_bitcount > 8) // If the image is more than 8 bit, shift values to convert preview to 8 bit range, for JPG encoding	
				new_preview_image.convertTo(new_preview_image, CV_8U, 1.0f / (1 << (m_bitcount - 8)));

			color_correction::linear_to_sRGB(new_preview_image);

			{
				std::lock_guard<std::mutex> lock(m_mutex_preview_image);
				preview_image = new_preview_image;
			}
		}

		// Store full resolution preview image
		{
			std::lock_guard<std::mutex> lock(m_mutex_large_preview_image);
			large_preview_image = img.clone();
			large_preview_image_black_point = black_level;
		}
	}

	//if (m_debug_in_capture_cycle)
	//	printf("%f\n", dt);

	// Recording
	if (m_recording)
	{
		if (m_waiting_for_trigger && !m_waiting_for_trigger_hold)
		{
			// If we are waiting for the trigger, look for a >200 ms gap in the timestamps
			if (dt > 0.2)
			{
				//std::cout << "Recording trigger found!" << std::endl;
				m_waiting_for_trigger = false;
			}

			m_waiting_delay -= dt;
			if (m_waiting_delay < 0.0)
			{
				std::cerr << "Trigger timeout" << std::endl;
				m_got_trigger_timeout = true;
				m_waiting_for_trigger = false;
			}
		}

		if (!m_waiting_for_trigger && !m_closing_recorders)
		{
			// Store first frame of each recordings
			if (recording_first_frame.empty())
			{
				recording_first_frame = img.clone();
				recording_first_frame_black_level = black_level;
			}				

			// Accumulate frames only if we are recording, and we are not waiting for the trigger
			for (auto& r : m_recorders)
			{
				r->append(img, frame_timestamp, black_level);

				if (r->buffers_used(BUFFER_ENCODING) > 0)
					m_encoding_buffers_used = r->buffers_used(BUFFER_ENCODING);
				if (r->buffers_used(BUFFER_WRITING) > 0)
					m_writing_buffers_used = r->buffers_used(BUFFER_WRITING);
			}

		}

		if (m_record_frames_remaining > 0 && m_recorders[0]->frame_count() >= m_record_frames_remaining)
			stop_recording();
	}

	m_image_counter++;

	//auto time2 = boost::posix_time::microsec_clock::local_time();
	//std::cout << "Time: " << (time2 - time1) << std::endl;
}

void Camera::remove_recording_hold()
{
	m_waiting_for_trigger_hold = false;
}

void Camera::start_recording(const std::vector<std::string>& folders, bool wait_for_trigger, int nb_frames)
{
	// Start recording frames to the specified file
	if (!recording())
	{
		recording_first_frame.release();
		m_last_summary.reset();
		m_record_frames_remaining = nb_frames;
		m_encoding_buffers_used = 0;
		m_writing_buffers_used = 0;

		if (nb_frames>0)
			m_recorders.push_back(std::make_shared<SimpleImageRecorder>(unique_id(), framerate(), m_width, m_height, m_bitcount, m_color_need_debayer, m_color_balance, folders));
		else
			m_recorders.push_back(std::make_shared<SimpleMovieRecorder>(unique_id(), framerate(), m_width, m_height, m_bitcount, folders));
		m_recorders.push_back(std::make_shared<MetadataRecorder>(unique_id(), framerate(), m_width, m_height, m_bitcount, m_color_need_debayer, m_color_balance, folders, this));


		m_got_trigger_timeout = false;
		m_closing_recorders = false;
		m_recording = true;
		m_waiting_delay = 3.0;
		m_waiting_for_trigger_hold = true;
		m_waiting_for_trigger = wait_for_trigger;

	}
}

void Camera::stop_recording()
{
	// Stop Recording and close file Frames
	// Returns a dictionary with all recording data

	m_last_summary.reset();

	if (recording())
	{
		m_closing_recorders = true;

		// Close all recorders
		for (auto& r : m_recorders)
			r->close();

		// Prepare rapidjson tree to accumulate all data about this capture
		shared_json_doc d(new rapidjson::Document());
		d->SetObject();
		d->AddMember("unique_id", rapidjson::Value(unique_id().c_str(), d->GetAllocator()), d->GetAllocator());

		// Get summary data from all recorders
		for (auto& r : m_recorders)
			r->summarize(d);
		m_recorders.clear();

		// Fill summary tree with data about this capture
		{
			rapidjson::Value d_camera(rapidjson::kObjectType);
			d_camera.AddMember("unique_id", rapidjson::Value(unique_id().c_str(), d->GetAllocator()), d->GetAllocator());
			d_camera.AddMember("model", rapidjson::Value(model().c_str(), d->GetAllocator()), d->GetAllocator());
			d_camera.AddMember("version", rapidjson::Value(version().c_str(), d->GetAllocator()), d->GetAllocator());
			d_camera.AddMember("effective_fps", m_effective_fps, d->GetAllocator());
			d_camera.AddMember("framerate", framerate(), d->GetAllocator());
			d_camera.AddMember("width", width(), d->GetAllocator());
			d_camera.AddMember("height", height(), d->GetAllocator());
			d_camera.AddMember("using_hardware_sync", using_hardware_sync(), d->GetAllocator());
			d_camera.AddMember("error_trigger_timeout", m_got_trigger_timeout, d->GetAllocator());
			
			d->AddMember("camera", d_camera, d->GetAllocator());

			if (!recording_first_frame.empty())
			{
				cv::Mat tempImage = recording_first_frame;

				if (m_color_need_debayer)
					cv::cvtColor(recording_first_frame, tempImage, cv::COLOR_BayerBG2RGB);

				cv::resize(tempImage, tempImage, cv::Size(m_preview_width*2, m_preview_height*2), 0.0, 0.0, cv::INTER_AREA);

				if (m_color_need_debayer)
					color_correction::apply(tempImage, m_color_balance, recording_first_frame_black_level);

				if (m_bitcount > 8) // If the image is more than 8 bit, shift values to convert preview to 8 bit range, for JPG encoding	
					tempImage.convertTo(tempImage, CV_8U, 1.0f / (1 << (m_bitcount - 8)));

				color_correction::linear_to_sRGB(tempImage);

				std::vector<unsigned char> buf;
				if (cv::imencode(".jpg", tempImage, buf))
				{
					d->AddMember("jpeg_thumbnail", rapidjson::Value(base64encode(buf).c_str(), d->GetAllocator()), d->GetAllocator());
				}
			}
			else if (!preview_image.empty())
			{
				std::vector<unsigned char> buf;
				if (get_preview_image(buf))
				{
					d->AddMember("jpeg_thumbnail", rapidjson::Value(base64encode(buf).c_str(), d->GetAllocator()), d->GetAllocator());
				}
			}

			m_last_summary = d;
			m_recording = false;
			m_waiting_for_trigger = false;
			m_waiting_for_trigger_hold = false;
			m_encoding_buffers_used = 0;
			m_writing_buffers_used = 0;
		}
	}
}

bool Camera::get_preview_image(std::vector<unsigned char>& buf)
{
	std::lock_guard<std::mutex> lock(m_mutex_preview_image);

	if (preview_image.empty())
		return false;

	return cv::imencode(".jpg", preview_image, buf); // cv2.IMWRITE_JPEG_QUALITY, 90
}
bool Camera::get_large_preview_image(std::vector<unsigned char>& buf)
{
	cv::Mat tempImage;

	{
		std::lock_guard<std::mutex> lock(m_mutex_large_preview_image);

		if (large_preview_image.empty())
			return false;

		bool is_copy = false; // if tempImage is a copy of large_preview_image

		if (m_color_need_debayer && large_preview_image.channels() == 1)
		{
			cv::cvtColor(large_preview_image, tempImage, cv::COLOR_BayerBG2RGB);
			color_correction::apply(tempImage, m_color_balance, large_preview_image_black_point);
			is_copy = true;
		}
		else
		{
			tempImage = large_preview_image;
		}

		if (m_bitcount > 8 && tempImage.depth() != CV_8U) // If the image is more than 8 bit, shift values to convert preview to 8 bit range, for JPG encoding
		{
			tempImage.convertTo(tempImage, CV_8U, 1.0f / (1 << (m_bitcount - 8)));
			is_copy = true;
		}

		if (!is_copy)
			tempImage = tempImage.clone();
	}

	color_correction::linear_to_sRGB(tempImage);
	return cv::imencode(".jpg", tempImage, buf); // cv2.IMWRITE_JPEG_QUALITY, 90
}

WebcamCamera::WebcamCamera(int id) : m_id(id)
{
	m_unique_id = "Webcam0";
	m_model = "Webcam";
	m_version = std::string("CV:") + CV_VERSION;

	m_cap = cv::VideoCapture(0);

	// read one frame to get frame size
	cv::Mat frame;
	m_cap >> frame;
	m_cap.release();

	m_width = frame.size[1];
	m_height = frame.size[0];

	m_framerate = 10;

	m_preview_width = default_preview_res;
	m_preview_height = m_preview_width*m_height / m_width; // keep aspect ratio for preview

	start_capture();
}

std::vector<std::shared_ptr<Camera> > WebcamCamera::get_webcam_cameras()
{
	std::vector<std::shared_ptr<Camera> > v;

	cv::VideoCapture cap(0); // open the default camera
	if (cap.isOpened())
	{
		cap.release();
		v.push_back(std::make_shared<WebcamCamera>(0));
	}

	return v;
}

void WebcamCamera::captureThread()
{
	cv::Mat frame;
	while (m_capturing)
	{
		m_cap >> frame;

		if (!frame.empty())
		{
			Camera::got_image(frame, 0, m_width, m_height, 8, 1);
		}
	}
}

void WebcamCamera::start_capture()
{
	if (!m_capturing)
	{
		m_cap = cv::VideoCapture(0);
		Camera::start_capture();

		capture_thread = boost::thread([this]() {captureThread(); });
	}
}

void WebcamCamera::stop_capture()
{
	if (m_capturing)
	{
		Camera::stop_capture();

		capture_thread.join();

		m_cap.release();
		m_effective_fps = 0;
	}
}
