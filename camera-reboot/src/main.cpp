
#include "alleg.h"

#include <base.h>
using namespace base;
using namespace std;

#include "capture.h"
#include "cam_pixels.h"
#include <iostream>

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
	CameraPixels::Frame f;
	if( !cam.open(1) ) {
		std::wcerr << L"open failed: " << cam.lastError() << L"\n";
		return 1;
	}

	FeatureDetector fd;

	while(!key[KEY_ESC] && !exited)
	{
		//cap.Grab();
		if( cam.readBGRA(f, 10) )
		{
			std::cout << "Frame " << f.width << "x" << f.height
				<< " stride=" << f.stride
				<< " bytes=" << f.data.size()
				<< " ts100ns=" << f.timestamp100ns << "\n";

			fd.Process(f);
			fd.ShowResult(buff, fd.output, 4);
		}

		//unsigned char *data;
		//while( !key[KEY_ESC] && !exited && !(data = cap.GetFrameData()) )
		//	rest(1);

		if(key[KEY_ESC] || exited)
			break;

		// int ch = 0;
		// if(keypressed())
		// 	ch = readkey()&0xFF;
		// if(ch==' ') set_topmost((topmost=!topmost));
		// if(ch=='1') draw_color = 0xFFFFFF, drawing_mode(DRAW_MODE_SOLID,NULL,0,0);
		// if(ch=='2') draw_color = 0x000000, drawing_mode(DRAW_MODE_SOLID,NULL,0,0);
		// if(ch=='3') draw_color = 0x808080, drawing_mode(DRAW_MODE_XOR,NULL,0,0);

		//for( int i=0; i<(int)cams.size(); i++ )
		//	textout_ex(buff,font,cams[i].c_str(),0,i*8,0xFFFFFF,-1);

		vsync();
		acquire_screen();
		blit(buff,screen,0,0,0,0,SCREEN_W,SCREEN_H);
		release_screen();

		rest(10);
	}

	return 0;
}
//END_OF_MAIN()
