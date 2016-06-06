#define _USE_MATH_DEFINES
#include <math.h>

#include "HoughHash.h"

#define MAX_ROTATION_ANGLE 20
#define ROTATION_STEPS 1
#define STEPS_RND 2

#define STEPS (2 * MAX_ROTATION_ANGLE + 1) / ROTATION_STEPS
#define HASH_MASK (HASH_SIZE - 1)

HoughHash::HoughHash()
:
maxcount(0),
maxScores(0, 0),
maxTransform(0, 0, 0)
{
	A = new float*[STEPS];
	
	for (int i = 0; i < STEPS; i++)
	{
        float a = static_cast<float>(ROTATION_STEPS * i - MAX_ROTATION_ANGLE);
		
		float cosa = float(cos(a * float(M_PI) / 180));
		float sina = float(sin(a * float(M_PI) / 180));

		A[i] = new float[4];

		A[i][0] = cosa;
		A[i][1] = sina;
		A[i][2] = -sina;
		A[i][3] = cosa;
	}
}

HoughHash::~HoughHash()
{
	for (int i = 0; i < STEPS; i++)
		delete[] A[i];
	delete[] A;
}

void HoughHash::reset()
{
	memset(hashmap, 0, HASH_SIZE * sizeof(hashmap[0]));
	memset(hashmap2D, 0, HASH_SIZE * sizeof(hashmap2D[0]));
	maxcount = 0;
	maxScores = cvPoint(0,0);
}

void HoughHash::fill(cv::Point2f p1, cv::Point2f p2, int score_or_punish)
{
	uint64 Key;
	cv::Point3f T;
	cv::Point3i rT;
	unsigned long hash;
	int count;
	int num_of_hits_with_same_score = 1;

	for (int i = 0; i < STEPS; i++)
	{
        T.z = static_cast<float>(ROTATION_STEPS*i - MAX_ROTATION_ANGLE);

		// A*p0 + t = p1 => A*p0-p1 = -t
		T.x = - (A[i][0]*p1.x + A[i][1]*p1.y - p2.x); //resulting translation (tx, ty)
		T.y = - (A[i][2]*p1.x + A[i][3]*p1.y - p2.y);

		rT = roundTransform(T);

		Key = makeKey(rT.x, rT.y, rT.z);

		hash = sdbm(reinterpret_cast<unsigned char*>(&Key), 8) & HASH_MASK ;

		hashmap[hash] += score_or_punish;
		
		if (score_or_punish > 0)
			hashmap2D[hash].x += score_or_punish;
		else
			hashmap2D[hash].y += -score_or_punish;

        count = static_cast<int>(hashmap2D[hash].x); //ignore penalties

		if (count >= maxcount)
		{
			if (count == maxcount) //then we need to average the transform parameters sharing the same amount of votes
			{
				//running average
				maxTransform *= num_of_hits_with_same_score;
				num_of_hits_with_same_score++;
				maxTransform = (maxTransform + T) * (1.0 / num_of_hits_with_same_score);
			}
			else
			{
				num_of_hits_with_same_score = 1;
				maxTransform = T;
				maxcount = count;
				maxScores = hashmap2D[hash];
			}
		}
	}
	
}

cv::Point3f HoughHash::getMaxTransform(int * score, cv::Point2f * scores)
{
	if(score) (*score) = maxcount;
	if(scores) (*scores) = maxScores;

	return unRoundTransform(roundTransform(maxTransform));
}


cv::Point3i HoughHash::roundTransform(cv::Point3f T)
{
	//actually we do not round, just shift the powers of ten
	cv::Point3i R;
	
	/*
		works like this:
		given a number of steps between 2 consecutive integers
		round q to the nearest step
		example:
		STEPS_RND = 5 
		=> 0.0, 0.2, 0.4, 0.6, 0.8 are the steps
		round(1.3) is rounded to 1.4
		*10 shifts the decade up to produce an int 
		(therefor, even if STEPS_RND was 3 the rest of the .33333... is cut)
		(step is usually 2)
		higher precision is usually not necessary since data is noisy 
		and there is not many data to account for that enough at high precision
		...
		what a long text in a little function
	
	*/

    R.x = static_cast<int>(10.0 * (static_cast<float>(cvRound(STEPS_RND * T.x)) / STEPS_RND));
    R.y = static_cast<int>(10.0 * (static_cast<float>(cvRound(STEPS_RND * T.y)) / STEPS_RND));
    R.z = static_cast<int>(10.0 * (static_cast<float>(cvRound(STEPS_RND * T.z)) / STEPS_RND));

	return R;
}

cv::Point3f HoughHash::unRoundTransform(cv::Point3i T)
{	
	cv::Point3f R;

	R.x = T.x / 10.f;
	R.y = T.y / 10.f;
	R.z = T.z / 10.f;

	return R;
}



unsigned long HoughHash::sdbm(unsigned char *str, int length)
{
    unsigned long hash = 0;
   
	for (int i = 0; i < length; i++)
		hash = str[i] + (hash << 6) + (hash << 16) - hash;

    return hash;
}



uint64 HoughHash::makeKey( int a,  int b,  int c)
{
	uint64 mask = 0x7FFFF; //19bit - mask
	uint64 minus = 0x80000; //20th bit set
	uint64 ret = 0;
	
	uint64 temp;
	
	temp = (a < 0) ? (unsigned(-a) & mask) | minus : (a & mask);
	ret = ret | temp;
	
	temp = (b < 0) ? (((unsigned(-b) & mask) | minus) << 20) : ((b & mask) << 20);
	ret = ret | temp;

	temp = (c < 0) ? (((unsigned(-c) & mask) | minus ) << 40)  : ((c & mask) << 40) | minus;
	ret = ret | temp;

	return ret;
}

