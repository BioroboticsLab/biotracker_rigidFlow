#include "OverlapOFTracker.h"
#include <random>
#define NUM_MODELS 40
#define MODEL_VARIANCE_XY 10
#define MODEL_VARIANCE_PHI 10

OverlapOFTracker::OverlapOFTracker()
:
correctionMask(Mask()),
sets(0),
points(NULL),
status(NULL),
error(NULL),
counter(0),
iterator_initialized(false)
{
}

OverlapOFTracker::~OverlapOFTracker()
{
	deInit();
}

void OverlapOFTracker::configure(int future_steps, int non_correction_steps, int features, bool ncs_enabled)
{
	sets = future_steps;
	OFTracker::configure(features/future_steps, ncs_enabled, non_correction_steps);
}

/*
* Initialises points, status and error matrices for each set of overlapping frames
*/
void OverlapOFTracker::init(cv::Mat &frame)
{
	points = new std::vector<cv::Point2f>*[sets];
	for (int i = 0; i < sets; i++)
	{
		points[i] = new std::vector<cv::Point2f>[sets];
		for (int j = 0; j < sets; j++)
			points[i][j] = std::vector<cv::Point2f>(nfeatures);
	}
    
	status = new std::vector<char>[sets];
	for (int i = 0; i < sets; i++){
		status[i] = std::vector<char>(nfeatures);
		for(int j = 0; j < nfeatures; j++)
			status[i][j] = 1;
	}

	error = new std::vector<float>[sets];
	for (int i = 0; i < sets; i++)
		error[i] = std::vector<float>(nfeatures);

	counter = 0;
	OFTracker::init(frame);
	
	correctionMask.init(frame.size());
}

/*
* Initialises points, status and error matrices for each set of overlapping frames
*/
void OverlapOFTracker::init(cv::Mat &frame, BeeBox &bb) {
    points = new std::vector<cv::Point2f>*[sets];
    for (int i = 0; i < sets; i++)
    {
        points[i] = new std::vector<cv::Point2f>[sets];
        for (int j = 0; j < sets; j++)
            points[i][j] = std::vector<cv::Point2f>(nfeatures);
    }

    status = new std::vector<char>[sets];
    for (int i = 0; i < sets; i++){
        status[i] = std::vector<char>(nfeatures);
        for(int j = 0; j < nfeatures; j++)
            status[i][j] = 1;
    }

    error = new std::vector<float>[sets];
    for (int i = 0; i < sets; i++)
        error[i] = std::vector<float>(nfeatures);

    counter = 0;
    OFTracker::init(frame, bb);

    correctionMask.init(frame.size());

    track();
}

void OverlapOFTracker::reset()
{
	deInit();
	OFTracker::reset();
}

void OverlapOFTracker::deInit()
{
	if (!isInitialized()) return;
	
	for (int i = 0; i < sets; i++)
		delete[] points[i];
	delete[] points;

	delete[] status;
	
	delete[] error;
}

/*
* Finds new features for the set determined by pos and tracks them through subsequent sets.
* This is the main function for the OverlapOFTracker.
*/
bool OverlapOFTracker::track()
{
	int pos = counter % sets;
	int D = (counter > sets) ? sets : counter;

	findFeatures(points[pos][pos], true);

	for (int i = 0; i < D; i++){ // iterate through point sets
		if (points[i][0].size() > 0){ // number of points in set i
			if (i != pos)
			{
				trackFeatures(points[i][(pos - 1 + sets) % sets], points[i][pos], status[i], error[i]);
			}
		}
	}
	
	counter++;
	return true;
}

/*
* generates random box orientations and checks if they suit they match the features better then the 
* calculated box for this step from the HoughHash
*/
void OverlapOFTracker::correct(BeeBox &bb)
{
	std::vector<BeeBox> models;
	cv::Point2f p1, p2;
	BeeBox temp;
	float score, last_max_score = LONG_MIN, max_score = 0;

	cv::Point3f max_transform, T;
	int max_index = 0;
	int iterations = 0;

	do
	{
		last_max_score = max_score;

		models = distributeModels(bb);
		max_score = LONG_MIN;
		
		//count and score for all random models 
		for (int m = 0; m < static_cast<int>(models.size()); m++)
		{
			score = 0; //overall score for one model
			temp = models[m]; // get the next model
            score = static_cast<float>(scoreModel(temp));

			//store the global max
			if (score > max_score)
			{
				max_score = score;
				max_index = m;
			}
		}

		bb = models[max_index];//overwrite input BB with max scores bbox

		iterations++;
	} while (max_score != last_max_score);
}

int OverlapOFTracker::scoreModel(BeeBox &bb)
{
	cv::Point2f p1, p2;
	cv::Point2f img_center = bb.getRotationCenter();

	float score = 0;
	int tmp_score;
	cv::Point3f T;
	int count;
	char c;

	for (int i = 0; i < sets - 1; i++)
	{
		count = 0;
		hough->reset();
		
		correctionMask.set(bb);
		while(iteratePoints(i, p1, p2))
		{ 
			c = correctionMask.getValue(p1);

			if (c == INSIDE_BEEBOX)
			{
				hough->fill(p1 - img_center, p2 - img_center, 1);
				count++;
			}
			else
				if (c == INSIDE_RIM)
				{
					hough->fill(p1-img_center, p2-img_center, -1);
					count++;
				}
		}
		
		if (count){
			bb.applyTransform(hough->getMaxTransform(&tmp_score));

			tmp_score = std::max(tmp_score, 0);
			score += static_cast<float>(tmp_score * tmp_score) / count;
		}
	}

    return static_cast<int>(score);
}

std::vector<BeeBox> OverlapOFTracker::distributeModels(BeeBox &seed)
{
	std::vector<BeeBox> models(NUM_MODELS);
	std::default_random_engine g;
	std::normal_distribution<> dx(seed.x, sqrt(MODEL_VARIANCE_XY));
	std::normal_distribution<> dy(seed.y, sqrt(MODEL_VARIANCE_XY));
	std::normal_distribution<> dp(0, sqrt(MODEL_VARIANCE_PHI));

	models[0] = seed;
	for (int i = 1; i < NUM_MODELS; i++){
        models[i] = BeeBox(static_cast<float>(dx(g)), static_cast<float>(dy(g)), seed.w, seed.h, seed.phi + static_cast<float>(dp(g)));
	}

	return models;	
}

/*
* iterator for the current time
*/
bool OverlapOFTracker::iteratePoints(cv::Point2f &p1, cv::Point2f &p2)
{
	return iteratePoints(sets - 2, p1, p2); //sets - 2 is the current position
}

/*
* iterator for arbitrary times.
* rPos = 0 is the oldest one, rPos = sets -2 the latest one.
*/
bool OverlapOFTracker::iteratePoints(int rPos, cv::Point2f &p1, cv::Point2f &p2)
{
	if (!iterator_initialized)
	{	
		iterator_pos = 0;
		iterator_initialized = true;	
	}

	int set = iterator_pos / nfeatures;
	int pnt = iterator_pos % nfeatures;
	int pos = counter + rPos + 1;
	
	if((set + 1) % sets == counter % sets)
	{
		set++;
		iterator_pos += nfeatures;
	}
	
	if(set >= sets || set >= counter)
	{
		iterator_initialized = false;
		return false;
	}
	
	p1 = points[set][(pos - 1 + sets) % sets][pnt];
	p2 = points[set][pos % sets][pnt];

	iterator_pos++;
	
	return true;
}
