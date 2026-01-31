
#pragma once


#include "alleg.h"

#include <math.h>
#include <vfw.h>



class CVideoCapture {
public:

	CVideoCapture(int _sx,int _sy,HWND hWnd,int xp,int yp,bool visible,int id,int device);
	~CVideoCapture();

	void Grab();
	unsigned char *GetFrameData()
	{
		if(!new_frame) return NULL;
		new_frame = false;
		return data;
	}

	// internal
	void CaptureFrame(PVIDEOHDR pvh);

private:
	int				size_x;
	int				size_y;
	HWND			cap_window;
	unsigned char	*data;
	bool			new_frame;

};
