#pragma once

#include <opencv2/core/core.hpp>

#include "FlowBox.h"

#define INSIDE_FlowBox 2
#define INSIDE_RIM 64

class Mask
{
public:
	cv::Mat mask;

	Mask();
	~Mask();
	
	void init(cv::Size size);
	void set(const FlowBox &bb);
	char getValue(const cv::Point2f p);

private:
	void drawBoundingBoxFilled(cv::Mat& img, const FlowBox bb, cv::Scalar color = cv::Scalar(255)) const;
};
