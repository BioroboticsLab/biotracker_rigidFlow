#pragma once
#include <opencv2/core/core.hpp>

#include "BeeBox.h"
#include "HoughHash.h"
#include "Mask.h"

typedef std::vector<cv::Point2f> Points;
typedef std::vector<char> Errors;
typedef std::vector<float> Statuses;

class OFTracker
{
protected:
	int nfeatures;
	HoughHash * hough;

private:
	Mask mask;
	cv::Mat frame;
	cv::Mat gray, prev_gray;
	cv::Size win_size;
	cv::TermCriteria term_crit;
	bool initialized;
	bool use_correction;
	int correct_in_X_frames;
	int num_of_non_correction_frames;

public:
	virtual ~OFTracker();
	virtual void init(cv::Mat &frame);
	virtual void init(cv::Mat &frame, BeeBox &bb);
	bool isInitialized() const;
	void next(cv::Mat &frame, BeeBox &bb);
	virtual void reset();

protected:
	OFTracker();
	void configure(int n, bool use_correction, int non_correction_steps = -1);
	
	
	bool setFrame(cv::Mat &frame);
	bool findFeatures(Points &points, bool masked);
	bool trackFeatures(Points &points_old, Points &points_new, Errors &error, Statuses &status);
	virtual void correct(BeeBox &bb) = 0;
	virtual bool track() = 0;
	virtual bool iteratePoints(cv::Point2f &p1, cv::Point2f &p2) = 0;
	
private:
	void deInit();
	void removeOutliers(Points &newp, Statuses &status);
	bool setMask(const BeeBox &bb);
};
