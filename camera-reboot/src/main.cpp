
#include "alleg.h"

#include <base.h>
using namespace base;
using namespace std;

#include "capture.h"
#include "cam_pixels.h"
#include <iostream>

#include "detector.h"



bool exited = false;

void close_fn()
{
	exited = true;
}


void draw_video(BITMAP *dst,unsigned char *data)
{
	for(int y=0;y<SCREEN_H;y++)
		for(int x=0;x<SCREEN_W;x++)
		{
			_putpixel32(dst,x,SCREEN_H-y-1,(data[2]<<16) | (data[1]<<8) | data[0]);
			data+=3;
		}
}

void draw_video_yuy2(BITMAP *dst,unsigned char *data)
{
	int r, g, b;
	for(int y=0;y<SCREEN_H;y++)
		for(int x=0;x<SCREEN_W;x+=2)
		{
#define PUTRGB(dx,r,g,b)	_putpixel32(dst,x+(dx),y,(((r)&0xFF00)<<8) | ((g)&0xFF00) | ((b)>>8));

			// R = 1.164(Y - 16) + 1.596(V - 128)
			// G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128)
			// B = 1.164(Y - 16)                   + 2.018(U - 128)
			b = 298*(int(data[0])-16) + 517*(int(data[1])-128);
			g = 298*(int(data[0])-16) - 208*(int(data[1])-128) - 100*(int(data[3])-128);
			r = 298*(int(data[0])-16) + 409*(int(data[3])-128);
			if(r<0) r = 0; if(r>65535) r = 65535;
			if(g<0) g = 0; if(g>65535) g = 65535;
			if(b<0) b = 0; if(b>65535) b = 65535;
			PUTRGB(0,r,g,b)

			b = 298*(int(data[2])-16) + 517*(int(data[1])-128);
			g = 298*(int(data[2])-16) - 208*(int(data[1])-128) - 100*(int(data[3])-128);
			r = 298*(int(data[2])-16) + 409*(int(data[3])-128);
			if(r<0) r = 0; if(r>65535) r = 65535;
			if(g<0) g = 0; if(g>65535) g = 65535;
			if(b<0) b = 0; if(b>65535) b = 65535;
			PUTRGB(1,r,g,b)

			data+=4;

#undef PUTRGB
		}
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
	if( !cam.open(0) ) {
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

			// f.pixels contains raw bytes in the chosen format.
			// RGB32 is typically B,G,R,X byte order.
			//draw_video(buff, &f.pixels[0]);

			if(0)
			{
				int cw = min(SCREEN_W, f.width);
				int ch = min(SCREEN_H, f.height);

				for( int y=0; y<ch; y++ )
				{
					uint8_t* data = &f.data[y * f.stride];
					for( int x=0; x<cw; x++ )
					{
						//_putpixel32(buff, x, y, (data[2]<<16) | (data[1]<<8) | data[0]);
						_putpixel32(buff, x, y, data[1]&0x80 ? 0xFFFFFF : 0);
						data+=4;
					}
				}
			}
			else
			{
				fd.Process(f);
				fd.ShowResult(buff, fd.output, 4);
			}
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

		//draw_video(buff,data);
//		draw_video_yuy2(buff,data);
		//	hline(buff,0,SCREEN_H/2,SCREEN_W,draw_color);
		//	vline(buff,SCREEN_W/2,0,SCREEN_H,draw_color);

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
