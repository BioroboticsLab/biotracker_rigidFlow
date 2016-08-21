#define _USE_MATH_DEFINES
#include <math.h>

#include <cereal/archives/json.hpp>
#include <cereal/types/polymorphic.hpp>

#include "FlowBox.h"

#define ROTATION_OFFSET 0

FlowBox::FlowBox(void)
:
x(0),
y(0),
w(0),
h(0),
phi(0)
{
}

FlowBox::FlowBox(std::shared_ptr<FlowBox> other)
{
	x = other.get()->x;
	y = other.get()->y;
	w = other.get()->w;
	h = other.get()->h;
	phi = other.get()->phi;
}

FlowBox::FlowBox(float posx, float posy, float width, float height, float angle)
{
	x = posx;
	y = posy;
	w = width;
	h = height;
	phi = angle;
}

FlowBox::~FlowBox()
{
}

cv::Point2f FlowBox::getRotationCenter() const
{
	return rotateVector(cv::Point2f(ROTATION_OFFSET * ROTATION_OFFSET / 2, 0), -phi) + cv::Point2f(x, y);
}

void FlowBox::applyTransform(const cv::Point3f t)
{
	x += t.x;
	y += t.y;
	rotate(t.z);	
}

void FlowBox::rotate(float da)
{
	if (ROTATION_OFFSET)
	{
		cv::Point2f mean = cv::Point2f(x, y);
		cv::Point2f COR = getRotationCenter();
	
		mean = rotateVector(mean - COR, da) + COR;

		x = mean.x;
		y = mean.y;	
	}

	phi = static_cast<float>(fmod(phi + da + 720, 360));

}

std::vector<cv::Point2i> FlowBox::getCornerPoints() const
{
	std::vector<cv::Point2i> pts(4);
	int w2 = static_cast<int>(w / 2);
	int h2 = static_cast<int>(h / 2);
	float p = phi * float(M_PI) / 180;

	float sinp = static_cast<float>(sin(p));
	float cosp = static_cast<float>(cos(p));

	pts[0] = cv::Point2i(static_cast<int>(x - cosp * w2 + sinp * h2), static_cast<int>(y + sinp * w2 + cosp * h2));
	pts[1] = cv::Point2i(static_cast<int>(x + cosp * w2 + sinp * h2), static_cast<int>(y - sinp * w2 + cosp * h2));
	pts[2] = cv::Point2i(static_cast<int>(x + cosp * w2 - sinp * h2), static_cast<int>(y - sinp * w2 - cosp * h2));
	pts[3] = cv::Point2i(static_cast<int>(x - cosp * w2 - sinp * h2), static_cast<int>(y + sinp * w2 - cosp * h2));
	
	return pts;
}

cv::Point2f FlowBox::rotateVector(const cv::Point2f p, float a) const
{
	float cosa, sina;
	cosa = static_cast<float>(cos(a * M_PI / 180));
	sina = static_cast<float>(sin(a * M_PI / 180));
	return cv::Point2f(cosa * p.x - sina * p.y, sina * p.x + cosa * p.y);
}

std::ostream& operator<<(std::ostream &os, const FlowBox &bb)
{
	os << "BB("<< bb.x << ", " << bb.y << " ; " << bb.phi << ")";
	return os;
}

CEREAL_REGISTER_TYPE(FlowBox)
