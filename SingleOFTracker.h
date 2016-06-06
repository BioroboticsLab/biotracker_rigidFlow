#pragma once

#include "OFTracker.h"

class SingleOFTracker : public OFTracker
{
private:
	std::vector<cv::Point2f> points[2];
	std::vector<char> status;
	std::vector<float> error;

	bool need_features;
	bool swap;

	bool iterator_initialized;
	int iterator_pos;

public:
	SingleOFTracker();
	~SingleOFTracker();
	void configure(int n, bool draw_features = false);
	virtual void init(cv::Mat &frame) override;
	virtual void reset() override;

protected:
	void deInit();
	virtual bool track()  override;
	virtual void correct(BeeBox&) override {}
	virtual bool iteratePoints(cv::Point2f &p1, cv::Point2f &p2) override;
};
