
#include "alleg.h"
#include "detector.h"
#include "sendkey.h"
#include "udp_sender.h"

#include <iostream>



void SendUDP(uint8_t index)
{
	static UdpSender udp(9571); // localhost:9571

	uint8_t payload[5] ={ 'M','A','S','K', index + '0' };
	int n = udp.sendBytes(payload, sizeof(payload));

	if( n < 0 )
	{
		std::cout << "UDP send failed: " << udp.lastError()
			<< " " << udp.lastErrorMessage() << "\n";
		return;
	}

	std::cout << "Sent " << n << " bytes\n";
}






int FeatureDetector::BoxAverage(int x, int y, int size)
{
	int r1 = (size+1)/2;
	int r2 = size/2;
	int x1 = max(0, x - r1);
	int y1 = max(0, y - r1);
	int x2 = min(width - 1, x + r2);
	int y2 = min(height - 1, y + r2);
	if( x1>=x2 || y1>=y2 )
		return 0;

	int boxsum = prefix_sum[x2 + y2*width] - prefix_sum[x2 + y1*width] - prefix_sum[x1 + y2*width] + prefix_sum[x1 + y1*width];
	int count = (x2 - x1) * (y2 - y1);

	return (boxsum + (count>>1)) / count;
}


uint32_t FeatureDetector::Process(CameraPixels::Frame& frame)
{
	uint32_t found_code = 0;

	// Init settings
	width = frame.width;
	height = frame.height;

	// Load values
	{
		values.resize(width * height);

		int index = 0;
		for( int y=0; y<height; y++ )
		{
			uint8_t* data = &frame.data[y * frame.stride];
			for( int x=0; x<width; x++ )
			{
				// BGR
				values[index++] = data[0] + (data[1] << 1) + data[2];
				data += 4;
			}
		}
	}

	// Compute prefix sum
	{
		prefix_sum.resize(values.size());

		int index = 0;
		for( int y=0; y<height; y++ )
		{
			int line_sum = 0;
			for( int x=0; x<width; x++ )
			{
				line_sum += values[index];
				prefix_sum[index] = line_sum + (y > 0 ? prefix_sum[index - width] : 0);
				index++;
			}
		}
	}

	// Compute test blur
	{
		output.resize(values.size());
		int index = 0;
		for( int y=0; y<height; y++ )
		{
			for( int x=0; x<width; x++ )
			{
				//int src = values[index];
				int src = BoxAverage(x, y, 5);
				int avg = BoxAverage(x, y, 47);
				//int out = 512;
				//if( src*2 < avg ) out = 0;
				//if( (1023-src)*2 < 1023-avg ) out = 1023;
				//output[index] = out;
				output[index] = src > avg ? 1023 : 0;
				index++;
			}
		}
	}

	// Detect the shapes
	markers.clear();
	markers.resize(values.size());

	//int step = 8;
	//for( int y=0; y<height; y+=step )
	//	for( int x=0; x<width; x+=step )
	//	{
	//		if( (x^y) & step )
	//			markers[x + y *width] = 1;
	//	}

	// Detect features
	feature_used.clear();
	feature_used.resize(values.size());
	features.clear();
	
	for( int x = 0; x < width; x++ )
	{
		feature_used[x] = 1;
		feature_used[x + (height-1)*width] = 1;
	}

	for( int y = 0; y < height; y++ )
	{
		feature_used[y * width] = 1;
		feature_used[(width-1) + y*width] = 1;
	}

	for( int start = 0; start < (int)values.size(); start++ )
	{
		if( feature_used[start] )
			continue;

		Feature f;
		f.color = output[start];
		f.midx = 0;
		f.midy = 0;
		f.x1 = width;
		f.y1 = height;
		f.x2 = 0;
		f.y2 = 0;
		f.volume = 0;
		
		task_list.clear();
		task_list.push_back(start);
		for( int ti=0; ti<(int)task_list.size(); ti++ )
		{
			int task = task_list[ti];
			int tx = task % width;
			int ty = task / width;
			f.midx += tx;
			f.midy += ty;
			if( tx < f.x1 ) f.x1 = tx;
			if( ty < f.y1 ) f.y1 = ty;
			if( tx > f.x2 ) f.x2 = tx;
			if( ty > f.y2 ) f.y2 = ty;
			f.volume++;

			if( output[task-1] == f.color && !feature_used[task-1] ) task_list.push_back(task-1), feature_used[task-1] = 1;
			if( output[task-width] == f.color && !feature_used[task-width] ) task_list.push_back(task-width), feature_used[task-width] = 1;
			if( output[task+1] == f.color && !feature_used[task+1] ) task_list.push_back(task+1), feature_used[task+1] = 1;
			if( output[task+width] == f.color && !feature_used[task+width] ) task_list.push_back(task+width), feature_used[task+width] = 1;
		}

		f.midx = (f.midx + (f.volume>>1)) / f.volume;
		f.midy = (f.midy + (f.volume>>1)) / f.volume;

		if( f.volume >= 5*5 && f.volume <= 24*24 )
		{
			markers[f.midx + f.midy*width] = 1;
			features.push_back(f);
		}
	}

	// Link features
	for( int i=0; i<(int)features.size(); i++ )
		if( features[i].color == 0 )
		{
			Feature f = features[i];
			int dist2 = f.volume * (4*4);
			int mindist2 = int(dist2 * 0.7f * 0.7f);
			int maxdist2 = int(dist2 * 1.35f * 1.35f);
			int minvol = int(f.volume * 0.8f * 0.8f);
			int maxvol = int(f.volume * 1.25f * 1.25f);
			int slots[2] ={ -1, -1 };

			for( int j=0; j<(int)features.size(); j++ )
			{
				Feature& f2 = features[j];
				if( f2.color != f.color || f2.volume < minvol || f2.volume > maxvol || i==j )
					continue;

				int dx = f2.midx - f.midx;
				int dy = f.midy - f2.midy;
				int len2 = dx*dx + dy*dy;
				if( len2 < mindist2 || len2 > maxdist2 )
					continue;

				int index = dx < 0 ? 0 : 1;
				dx = abs(dx);
				if( dx*2 < dy || dy*2 < dx ) continue;

				slots[index] = j;
				markers[f2.midx + f2.midy*width] = 0xFFFF00;
			}

			if( slots[0]>=0 || slots[1]>=0 )
				markers[f.midx + f.midy*width] = 0x00FFFF;
			
			if( slots[0]>=0 && slots[1]>=0 )
			{
				int dx1 = features[slots[0]].midx - f.midx;
				int dy1 = features[slots[0]].midy - f.midy;
				int dx2 = features[slots[1]].midx - f.midx;
				int dy2 = features[slots[1]].midy - f.midy;

				if( f.midx + dy1 + dy2 < 0 )
					continue;

				int code = 0;
				int mask = 1;
				for( int py=0; py<5; py++ )
				{
					int rx = f.midx + ((dx1 * py + 2)>>2);
					int ry = f.midy + ((dy1 * py + 2)>>2);

					for( int px=0; px<5; px++ )
					{
						int fx = rx + ((dx2 * px + 2)>>2);
						int fy = ry + ((dy2 * px + 2)>>2);

						if(uint32_t(fx) < width && uint32_t(fy) < height)
						{
							int addr = fx + fy*width;
							if( output[addr] )
								code |= mask;
							markers[addr] = 0x00FF00;
						}
						mask <<= 1;
					}
				}

				const int code_mask_0 = 0x025836A;		// 00010 01011 00000 11011 01010  ->  0 0010 0101 1000 0011 0110 1010
				const int code_mask_1 = 0x0AD836A;		// 01010 11011 00000 11011 01010  ->  0 1010 1101 1000 0011 0110 1010
				const int code_mask_2 = 0x027A36A;		// 00010 01111 01000 11011 01010  ->  0 0010 0111 1010 0011 0110 1010
				const int code_mask_3 = 0x0258FEE;		// 00010 01011 00011 11111 01110  ->  0 0010 0101 1000 1111 1110 1110
				if( code == code_mask_0 )
				{
					printf("================ MASK 0 DETECTED! ================\n");
					SendUDP(0);
				}
				else if( code == code_mask_1 )
				{
					printf("================ MASK 1 DETECTED! ================\n");
					SendUDP(1);
				}
				else if( code == code_mask_2 )
				{
					printf("================ MASK 2 DETECTED! ================\n");
					SendUDP(2);
				}
				else if( code == code_mask_3 )
				{
					printf("================ MASK 3 DETECTED! ================\n");
					SendUDP(3);
				}
			}
		}

	return found_code;
}


void FeatureDetector::ShowResult(BITMAP* buff, vector<int>& data, int shift)
{
	int cw = min(buff->w, width);
	int ch = min(buff->h, height);

	for( int y=0; y<ch; y++ )
	{
		int* src = &data[y * width];
		int* msrc = &markers[y * width];
		for( int x=0; x<cw; x++ )
		{
			int color = ((*src++ >> shift) & 0xFF) * 0x010101;
			int marker = *msrc++;
			if( marker )
			{
				if( marker == 1 )
					color = color ? 0x008000 : 0x800000;
				else
					color = marker;
			}

			_putpixel32(buff, x, y, color);
		}
	}
}
