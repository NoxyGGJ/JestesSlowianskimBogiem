#include "alleg.h"

#include <base.h>
using namespace base;
using namespace std;

#include "capture.h"
#include "cam_pixels.h"
#include <cmath>
#include <iostream>
#include <vector>

#include "detector.h"
#include "sendkey.h"

bool exited = false;

void close_fn()
{
	exited = true;
}

void set_topmost(bool topmost)
{
	HWND wnd = win_get_window();
	if(topmost)	SetWindowPos(wnd,HWND_TOPMOST  ,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
	else		SetWindowPos(wnd,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);

	set_window_title(topmost ? "Camera [Top]" : "Camera");
}

static bool ScaleBGRABoxAverageToFit(const CameraPixels::Frame& src, int maxW, int maxH, CameraPixels::Frame& dst)
{
	if( src.format != CameraPixels::PixelFormat::BGRA32 || src.width <= 0 || src.height <= 0 || src.stride < src.width * 4 )
		return false;
	if( maxW <= 0 || maxH <= 0 )
		return false;

	double scaleX = double(maxW) / double(src.width);
	double scaleY = double(maxH) / double(src.height);
	double scale = min(1.0, min(scaleX, scaleY));

	int dstW = max(1, int(floor(src.width  * scale + 1e-9)));
	int dstH = max(1, int(floor(src.height * scale + 1e-9)));

	if( dstW == src.width && dstH == src.height ) {
		dst = src;
		return true;
	}

	vector<float> temp((size_t)src.height * (size_t)dstW * 4u, 0.0f);

	for( int y = 0; y < src.height; ++y )
	{
		const uint8_t* row = &src.data[(size_t)y * src.stride];
		for( int dx = 0; dx < dstW; ++dx )
		{
			double sx0 = double(dx) * src.width / dstW;
			double sx1 = double(dx + 1) * src.width / dstW;
			int ix0 = max(0, int(floor(sx0)));
			int ix1 = min(src.width, int(ceil(sx1)));
			double accum[4] = { 0.0, 0.0, 0.0, 0.0 };
			for( int sx = ix0; sx < ix1; ++sx )
			{
				double left = max(sx0, double(sx));
				double right = min(sx1, double(sx + 1));
				double weight = right - left;
				if( weight <= 0.0 )
					continue;

				const uint8_t* px = row + sx * 4;
				accum[0] += px[0] * weight;
				accum[1] += px[1] * weight;
				accum[2] += px[2] * weight;
				accum[3] += px[3] * weight;
			}

			double norm = sx1 - sx0;
			if( norm <= 0.0 )
				norm = 1.0;

			size_t base = ((size_t)y * dstW + dx) * 4u;
			temp[base + 0] = float(accum[0] / norm);
			temp[base + 1] = float(accum[1] / norm);
			temp[base + 2] = float(accum[2] / norm);
			temp[base + 3] = float(accum[3] / norm);
		}
	}

	dst = {};
	dst.format = CameraPixels::PixelFormat::BGRA32;
	dst.rowOrder = CameraPixels::RowOrder::TopDown;
	dst.width = dstW;
	dst.height = dstH;
	dst.stride = dstW * 4;
	dst.timestamp100ns = src.timestamp100ns;
	dst.data.resize((size_t)dst.stride * (size_t)dst.height);

	for( int dy = 0; dy < dstH; ++dy )
	{
		double sy0 = double(dy) * src.height / dstH;
		double sy1 = double(dy + 1) * src.height / dstH;
		int iy0 = max(0, int(floor(sy0)));
		int iy1 = min(src.height, int(ceil(sy1)));
		uint8_t* dstRow = &dst.data[(size_t)dy * dst.stride];

		for( int x = 0; x < dstW; ++x )
		{
			double accum[4] = { 0.0, 0.0, 0.0, 0.0 };
			for( int sy = iy0; sy < iy1; ++sy )
			{
				double top = max(sy0, double(sy));
				double bottom = min(sy1, double(sy + 1));
				double weight = bottom - top;
				if( weight <= 0.0 )
					continue;

				size_t base = ((size_t)sy * dstW + x) * 4u;
				accum[0] += temp[base + 0] * weight;
				accum[1] += temp[base + 1] * weight;
				accum[2] += temp[base + 2] * weight;
				accum[3] += temp[base + 3] * weight;
			}

			double norm = sy1 - sy0;
			if( norm <= 0.0 )
				norm = 1.0;

			dstRow[x * 4 + 0] = uint8_t(accum[0] / norm + 0.5);
			dstRow[x * 4 + 1] = uint8_t(accum[1] / norm + 0.5);
			dstRow[x * 4 + 2] = uint8_t(accum[2] / norm + 0.5);
			dstRow[x * 4 + 3] = uint8_t(accum[3] / norm + 0.5);
		}
	}

	return true;
}

#undef main
int main()
{
	//CoInitialize(NULL);

	auto devs = CameraPixels::enumerate();
	for( size_t i = 0; i < devs.size(); ++i )
		std::wcout << i << L": " << devs[i] << L"\n";

	allegro_init();
	install_keyboard();
	install_mouse();

	set_window_title("Camera");
	set_close_button_callback(close_fn);

	set_color_depth(32);
	set_gfx_mode(GFX_AUTODETECT_WINDOWED,640,480,0,0);
	BITMAP *buff = create_bitmap(SCREEN_W,SCREEN_H);
	clear(buff);

	show_mouse(screen);

	if(set_display_switch_mode(SWITCH_BACKAMNESIA)!=0)
		set_display_switch_mode(SWITCH_BACKGROUND);

#if 0
	vector<string> cams;
	for(int i=0;i<10;i++) 
	{
		char b1[100],b2[100];
		if(capGetDriverDescription(i,b1,sizeof(b1),b2,sizeof(b2))) 
		{
			cams.push_back(format("%d: %s, %s",i,b1,b2));
			printf("%s\n",cams[cams.size()-1].c_str());
		}
	} 

	CVideoCapture cap(SCREEN_W,SCREEN_H,win_get_window(),0,0,false,0,0);
	bool topmost = true;
	int draw_color = 0xFFFFFF;
	set_topmost(true);
#endif

	CameraPixels cam;
	CameraPixels::Frame cameraFrame;
	CameraPixels::Frame processedFrame;
	if( !cam.open(0) ) {
		std::wcerr << L"open failed: " << cam.lastError() << L"\n";
		return 1;
	}

	FeatureDetector fd;

	while(!key[KEY_ESC] && !exited)
	{
		if( cam.readBGRA(cameraFrame, 10) )
		{
			if( !ScaleBGRABoxAverageToFit(cameraFrame, SCREEN_W, SCREEN_H, processedFrame) )
				processedFrame = cameraFrame;

			std::cout << "Frame " << processedFrame.width << "x" << processedFrame.height
				<< " stride=" << processedFrame.stride
				<< " bytes=" << processedFrame.data.size()
				<< " ts100ns=" << processedFrame.timestamp100ns << "\n";

			fd.Process(processedFrame);
			fd.ShowResult(buff, fd.output, 4);
		}

		if(key[KEY_ESC] || exited)
			break;

		vsync();
		acquire_screen();
		blit(buff,screen,0,0,0,0,SCREEN_W,SCREEN_H);
		release_screen();

		rest(10);
	}

	return 0;
}
//END_OF_MAIN()
