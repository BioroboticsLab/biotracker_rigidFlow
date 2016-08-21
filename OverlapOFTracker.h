#pragma once

#include "OFTracker.h"

class OverlapOFTracker : public OFTracker
{
private:
	Mask correctionMask;
	int sets; // == future steps
	std::vector<cv::Point2f> **points;
	std::vector<char> *status;
	std::vector<float> *error;
	int counter;
	
	bool iterator_initialized;
	int iterator_pos;

public:
	OverlapOFTracker();
	~OverlapOFTracker();
	void configure(int future_steps, int non_correction_steps, int features, bool ncs_enabled);
	virtual void init(cv::Mat &frame) override;
	virtual void init(cv::Mat &frame, FlowBox &bb) override;
	virtual void reset() override;

protected:
	virtual bool track()  override;
	virtual bool iteratePoints(cv::Point2f &p1, cv::Point2f &p2) override;
	bool iteratePoints(int pos, cv::Point2f &p1, cv::Point2f &p2);

private:
	void deInit();
	virtual void correct(FlowBox &bb) override;
	int scoreModel(FlowBox &bb);
	std::vector<FlowBox> distributeModels(FlowBox &seed);
};
