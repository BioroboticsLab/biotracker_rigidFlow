#pragma once

#include <opencv2/core/core.hpp>

#define HASH_SIZE 131072

class HoughHash
{
	int maxcount;
	cv::Point2f maxScores;
	cv::Point3f maxTransform;
	float ** A; // M 2x2 matrices written as vector (a11 a12 a21 a22)
	
	int hashmap[HASH_SIZE];
	cv::Point2f hashmap2D[HASH_SIZE];
	
public:
	HoughHash();
	~HoughHash();
	void fill(cv::Point2f p1, cv::Point2f p2, int score_or_punish);
	void reset();
	cv::Point3f getMaxTransform(int * score = 0, cv::Point2f * scores = 0);
	uint64 makeKey(int a, int b, int c);
	unsigned long sdbm(unsigned char *str, int length); //hash function
	cv::Point3i roundTransform(cv::Point3f T);
	cv::Point3f unRoundTransform(cv::Point3i T);
};
