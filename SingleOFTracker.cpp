#include "SingleOFTracker.h"

SingleOFTracker::SingleOFTracker()
:
need_features(true),
swap(false),
iterator_initialized(false),
iterator_pos(0)
{
	points[0] = std::vector<cv::Point2f>();
	points[1] = std::vector<cv::Point2f>();
	status = std::vector<char>();
	error = std::vector<float>();
}

SingleOFTracker::~SingleOFTracker()
{
	deInit();
}

void SingleOFTracker::configure(int n)
{
	if(n != nfeatures) need_features = true;
	OFTracker::configure(n, false, -1);
}

void SingleOFTracker::init(cv::Mat &frame)
{
	points[0].resize(nfeatures);
	points[1].resize(nfeatures);
	error.resize(nfeatures);
	status.resize(nfeatures);

	OFTracker::init(frame);
}

void SingleOFTracker::init(cv::Mat &frame, FlowBox &bb)
{
    points[0].resize(nfeatures);
    points[1].resize(nfeatures);
    error.resize(nfeatures);
    status.resize(nfeatures);

    OFTracker::init(frame, bb);

    if (need_features) {
        if(findFeatures(points[1], true))
        {
            need_features = false;
            swap = false;
        }
    }
}

void SingleOFTracker::reset()
{
	deInit();
	OFTracker::reset();
}

void SingleOFTracker::deInit()
{
	if(!isInitialized()) return;

	need_features = true;

	points[0].clear();
	points[1].clear();
	error.clear();
	status.clear();
}

bool SingleOFTracker::track()
{
	if (need_features) {
		if(findFeatures(points[1], true))
		{
			need_features = false;
			swap = false;
			return true;
		}
		return false;
	}
	else {
		bool r = trackFeatures(points[!swap], points[swap], status, error);
		swap = !swap;
		return r;
	}	
}


bool SingleOFTracker::iteratePoints(cv::Point2f &p1, cv::Point2f &p2)
{
	if(!isInitialized() || need_features) return false;

	if(!iterator_initialized)
	{
		iterator_initialized = true;
		iterator_pos = 0;
	}
	
	if(iterator_pos == static_cast<int>(points[0].size()))
	{
		iterator_initialized = false;
		return false;
	}
	
	p1 = points[swap][iterator_pos];
	p2 = points[!swap][iterator_pos];
	
	iterator_pos++;
	
	return true;
}
