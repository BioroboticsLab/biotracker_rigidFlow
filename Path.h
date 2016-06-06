#pragma once

#include "BeeBox.h"

#include <biotracker/serialization/TrackedObject.h>

class Path {
  public:
	Path(int frame);
	Path(BioTracker::Core::TrackedObject to);
	~Path();
	void add(BeeBox bb);
	void deleteLast();
	BeeBox operator [](int frame) const;
	BeeBox& operator [](int frame);
	bool isInRange(int frame);
	bool isEmpty();
	int size();
	int begin();
	int end();
	BioTracker::Core::TrackedObject toTrackedObject(int id);

  private:
	int startframe;
	int endframe;
	std::vector<BeeBox> boxes;
};

