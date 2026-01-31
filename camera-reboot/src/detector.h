
#pragma once

#include <base.h>
using namespace base;
using namespace std;

#include "cam_pixels.h"


struct BITMAP;



//class DataImage {
//public:
//};



struct Feature {
	int color;
	int midx, midy;
	int x1, y1, x2, y2;
	int volume;
};


class FeatureDetector {
public:
	vector<int> values;
	vector<int> prefix_sum;
	vector<int> output;
	vector<int> markers;
	vector<int> feature_used;
	vector<Feature> features;
	vector<int> task_list;
	int width = 0;
	int height = 0;

	// Helpers
	int BoxAverage(int x, int y, int size);


	// Main entry
	uint32_t Process(CameraPixels::Frame& frame);

	void ShowResult(BITMAP* buff, vector<int>& data, int shift);
};
