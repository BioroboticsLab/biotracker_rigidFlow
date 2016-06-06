#include "Mask.h"

Mask::Mask()
{
}

Mask::~Mask()
{
	mask.release();
}

void Mask::init(cv::Size size)
{
	mask = cv::Mat::zeros(size, CV_8UC1);
}
	
void Mask::set(const BeeBox &bb)
{
	mask.setTo(0);

	if(bb.w == 0 || bb.h == 0) return;

	BeeBox large(bb);

	large.w = bb.w * 2;
	large.h = bb.h * 1.5f;

	drawBoundingBoxFilled(mask, large, cv::Scalar(INSIDE_RIM));
	drawBoundingBoxFilled(mask, bb, cv::Scalar(INSIDE_BEEBOX));
}

char Mask::getValue(const cv::Point2f p)
{
	if (p.x < 0 || p.y < 0 || static_cast<int>(p.x) > mask.cols || static_cast<int>(p.y) > mask.rows) return 0;
	return mask.at<unsigned char>(static_cast<int>(p.y), static_cast<int>(p.x));	
}

void Mask::drawBoundingBoxFilled(cv::Mat& img, const BeeBox bb, cv::Scalar color) const
{
	std::vector<cv::Point> pnts = bb.getCornerPoints();
	cv::fillConvexPoly(img, &pnts[0], 4, color);
}
