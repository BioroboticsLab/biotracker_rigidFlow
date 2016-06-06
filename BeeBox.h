#pragma once

#include <cereal/access.hpp>

#include <biotracker/serialization/TrackedObject.h>

class BeeBox : public BioTracker::Core::ObjectModel {
public:
	float x, y, w, h;
	float phi;

	BeeBox(void);
//	BeeBox(const BeeBox &other);
	BeeBox(std::shared_ptr<BeeBox> other);
	BeeBox(float x, float y, float w, float h, float p);
	~BeeBox();
	cv::Point2f getRotationCenter() const;
	void applyTransform(const cv::Point3f t);
	void rotate(float da);
	std::vector<cv::Point2i> getCornerPoints() const;

private:
	cv::Point2f rotateVector(const cv::Point2f p, float a) const;

	friend class cereal::access;
	template <class Archive>
	void serialize(Archive& ar)
	{
		ar(CEREAL_NVP(x),
			CEREAL_NVP(y),
			CEREAL_NVP(w),
			CEREAL_NVP(h),
			CEREAL_NVP(phi));
	}
};

std::ostream& operator<<(std::ostream &os, const BeeBox &bb);
