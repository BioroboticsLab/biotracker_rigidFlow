#include "Path.h"

Path::Path(int frame) {
	startframe = frame;
}
Path::Path(BioTracker::Core::TrackedObject to) {
	boost::optional<size_t> max;
	startframe = -1;
	if (!(max = to.getLastFrameNumber())) return;
	for (int i = 0; i < static_cast<int>(*max); i++) {
		if (to.count(i)) {
			if (startframe < 0) startframe = i;
			std::shared_ptr<BeeBox> bb = std::dynamic_pointer_cast<BeeBox>(to.get(i));
			add(*bb);
		}				
	}	
}

Path::~Path()
{
}

void Path::add(BeeBox bb)
{
	boxes.push_back(bb);
}
void Path::deleteLast() {
	boxes.pop_back();
}

BeeBox Path::operator [](int frame) const
{
	return boxes[frame - startframe];
}

BeeBox& Path::operator [](int frame)
{
	return boxes[frame - startframe];
}

bool Path::isInRange(int frame)
{
	return (frame - startframe >= 0) && frame - startframe < static_cast<int>(boxes.size());
}

bool Path::isEmpty()
{
	return boxes.size() < 2;
}
int Path::size() {
	return boxes.size();
}

int Path::begin()
{
	return startframe;
}

int Path::end()
{
	return isEmpty() ? startframe : startframe + boxes.size() - 1;
}

BioTracker::Core::TrackedObject Path::toTrackedObject(int id)
{
	BioTracker::Core::TrackedObject to = BioTracker::Core::TrackedObject(id);
	for (int i = 0; i < static_cast<int>(boxes.size()); i++) {
		to.add(startframe + i, std::shared_ptr<BioTracker::Core::ObjectModel>(new BeeBox(boxes[i])));
	}
	return to;
}


