#include "OFTracker.h"

#define FEATURE_COLOR 0, 255, 255

OFTracker::OFTracker()
:
nfeatures(200),
hough(NULL),
mask(Mask()),
gray(cv::Mat()),
prev_gray(cv::Mat()),
win_size(10, 10),
term_crit(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 40, 0.03),
initialized(false),
use_correction(false),
correct_in_X_frames(0),
num_of_non_correction_frames(0)
{
}

OFTracker::~OFTracker()
{
	deInit();
}

/*
* Configures the tracker.
* Can be called at any time.
*/
void OFTracker::configure(int n, bool use_correction, int non_correction_steps)
{
	nfeatures = n;
	
	this->use_correction = use_correction;

	if(use_correction)
	{
		num_of_non_correction_frames = non_correction_steps;
		correct_in_X_frames = non_correction_steps;
	}
}

/*
* Initialises mask and HoughHash.
* Must be called before using the tracker.
*/
void OFTracker::init(cv::Mat &frame)
{
	mask.init(frame.size());
	cv::cvtColor(frame, gray, CV_BGR2GRAY);
	cv::cvtColor(frame, prev_gray, CV_BGR2GRAY);
	
	this->frame = frame;
	
	hough = new HoughHash();

	correct_in_X_frames = num_of_non_correction_frames;
	initialized = true;
}

void OFTracker::deInit()
{
	if (!initialized) return;

	gray.release();
	prev_gray.release();
	delete hough;

	initialized = false;
}

void OFTracker::reset()
{
	deInit();
}

bool OFTracker::isInitialized() const
{
	return initialized;
}

bool OFTracker::setMask(const BeeBox &bb)
{
	if(!initialized) return false;
	
	mask.set(bb);
	
	return true;
}

bool OFTracker::setFrame(cv::Mat &frame)
{
	if(!initialized) return false;
	
	this->frame = frame;
	
	cv::Mat tmp;
	CV_SWAP(prev_gray, gray, tmp);
	
	cv::cvtColor(frame, gray, CV_BGR2GRAY);
	
	return true;
}

/*
* Finds trackable features in the area defined by our mask and bounding box
*/
bool OFTracker::findFeatures(Points &points, bool masked)
{
	if(!initialized) return false;
	
	cv::Mat msk;
	if (masked) msk = mask.mask;
	else msk = cv::Mat();
	
	double quality = 0.01;
	double min_distance = 3;

	cv::goodFeaturesToTrack(gray, points, nfeatures, quality, min_distance, msk, 3, 0, 0.04);

	if (points.size() == 0) return false;
	
	cv::cornerSubPix(gray, points, win_size, cv::Size(-1,-1), term_crit);
	return true;
}
/*
* Calculates the movement of all features in the bounding box between the current and previous step
*/
bool OFTracker::trackFeatures(Points &points_old, Points &points_new, Errors &error, Statuses &status)
{
	if(!initialized) return false;

	//hack, remove if possible
	cv::Mat in(points_old), out(points_new);
	cv::Mat stat(status);
	cv::Mat err(error);
	
	cv::calcOpticalFlowPyrLK(prev_gray, gray, in, out, stat, err, win_size, 3, term_crit, 0);

	if(points_new.size() != points_old.size()) points_new.resize(points_old.size());
	for(int i = 0; i < static_cast<int>(points_old.size()); i++)
	{
		points_new[i] = out.at<cv::Point2f>(i);
		status[i] = out.at<char>(i);
	}

	removeOutliers(points_new, status);
	return true;
}

/*
* Removes all points that lie outside of the bounding box from the mask
*/
void OFTracker::removeOutliers(Points &points, Statuses &status)
{
	for(int i = 0; i < static_cast<int>(points.size()); i++)
	{
		if(!status[i] || points[i].x >= gray.cols || points[i].x < 0 || points[i].y >= gray.rows || points[i].y < 0)
		{
			points[i] = cvPoint2D32f(-1, -1);
			status[i] = 0;
		}
	}
}

/*
* Calculates the position and orientation of the bounding box for the next frame using the Houghhash
* and the optical flow of all features determined by track() and trackFeatures(). If enabled, correction
* is performed afterwards.
* This is where all the interesting stuff either happens or is initiated from.
*/
void OFTracker::next(cv::Mat &frame, BeeBox &bb)
{
	if (!initialized) return;

	cv::Point2f p1, p2;
	cv::Point2f center = bb.getRotationCenter();

	setMask(bb);
	setFrame(frame);
	track();
	
	hough->reset();

	int i = 0;

	//iterate through point moves
	while(iteratePoints(p1, p2)) 
	{
		if (mask.getValue(p1) == INSIDE_BEEBOX && mask.getValue(p2))
		{
			hough->fill(p1 - center, p2 - center, 1);
			i++;
		}
	}

	if(i) bb.applyTransform(hough->getMaxTransform());

	if(use_correction)
	{
		correct_in_X_frames--;

		if (correct_in_X_frames == 0)
		{
			correct(bb);
			correct_in_X_frames = num_of_non_correction_frames;
		}
	}
}
