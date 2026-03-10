#include "alleg.h"

#include <base.h>
using namespace base;
using namespace std;

#include "capture.h"
#include "cam_pixels.h"
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

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

static void ComputeFitRect(int srcW, int srcH, int maxW, int maxH, int& dstX, int& dstY, int& dstW, int& dstH)
{
	dstX = 0;
	dstY = 0;
	dstW = 0;
	dstH = 0;
	if( srcW <= 0 || srcH <= 0 || maxW <= 0 || maxH <= 0 )
		return;

	double scaleX = double(maxW) / double(srcW);
	double scaleY = double(maxH) / double(srcH);
	double scale = min(1.0, min(scaleX, scaleY));

	dstW = max(1, int(floor(srcW * scale + 1e-9)));
	dstH = max(1, int(floor(srcH * scale + 1e-9)));
	dstX = max(0, (maxW - dstW) / 2);
	dstY = max(0, (maxH - dstH) / 2);
}

struct CameraIdentity {
	wstring name;
	int ordinal;

	CameraIdentity()
		: ordinal(-1)
	{
	}

	bool isValid() const
	{
		return ordinal >= 0 && !name.empty();
	}
};

struct UiState {
	vector<wstring> enumeratedCameras;
	int openedCameraIndex;
	int lastKnownCameraIndex;
	int highlightedCameraIndex;
	CameraIdentity activeIdentity;
	wstring currentCameraName;
	bool multipleCamerasAvailable;
	DWORD lastEnumerationTick;
	DWORD lastFrameTick;
	bool disconnected;
	bool awaitingFirstFrame;
	wstring statusLine;

	UiState()
		: openedCameraIndex(-1)
		, lastKnownCameraIndex(-1)
		, highlightedCameraIndex(-1)
		, multipleCamerasAvailable(false)
		, lastEnumerationTick(0)
		, lastFrameTick(0)
		, disconnected(false)
		, awaitingFirstFrame(false)
	{
	}
};

static DWORD NowMs()
{
	return GetTickCount();
}

static CameraIdentity MakeCameraIdentity(const vector<wstring>& devices, int index)
{
	CameraIdentity id;
	if( index < 0 || index >= (int)devices.size() )
		return id;

	id.name = devices[index];
	id.ordinal = 0;
	for( int i = 0; i < index; ++i )
		if( devices[i] == id.name )
			++id.ordinal;
	return id;
}

static int FindCameraIndexByIdentity(const vector<wstring>& devices, const CameraIdentity& id)
{
	if( !id.isValid() )
		return -1;

	int ordinal = 0;
	for( int i = 0; i < (int)devices.size(); ++i )
	{
		if( devices[i] != id.name )
			continue;
		if( ordinal == id.ordinal )
			return i;
		++ordinal;
	}

	return -1;
}

static string NarrowForUi(const wstring& text)
{
	if( text.empty() )
		return string();

	int size = WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, NULL, 0, "?", NULL);
	if( size <= 1 )
		return string(text.begin(), text.end());

	vector<char> buffer((size_t)size, '\0');
	WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, &buffer[0], size, "?", NULL);
	return string(&buffer[0]);
}

static wstring BuildStatusWithError(const wstring& prefix, const wstring& error)
{
	if( error.empty() )
		return prefix;
	return prefix + L": " + error;
}

static void DrawUiLine(BITMAP* buff, int x, int y, int color, const wstring& text)
{
	string narrow = NarrowForUi(text);
	textout_ex(buff, font, narrow.c_str(), x, y, color, makecol(0, 0, 0));
}

static void BeginCameraAttempt(UiState& ui, int highlightedIndex)
{
	ui.highlightedCameraIndex = highlightedIndex;
	ui.awaitingFirstFrame = highlightedIndex >= 0;
	ui.disconnected = false;
}

static bool OpenCameraAtIndex(CameraPixels& cam, UiState& ui, int index, DWORD now)
{
	if( index < 0 || index >= (int)ui.enumeratedCameras.size() )
		return false;
	if( !cam.open(index) )
		return false;

	ui.activeIdentity = MakeCameraIdentity(ui.enumeratedCameras, index);
	ui.openedCameraIndex = index;
	ui.lastKnownCameraIndex = index;
	ui.highlightedCameraIndex = index;
	ui.currentCameraName = ui.enumeratedCameras[index];
	ui.lastFrameTick = now;
	ui.disconnected = false;
	ui.awaitingFirstFrame = true;
	return true;
}

static vector<int> BuildWrappedIndices(int count, int startIndex)
{
	vector<int> order;
	if( count <= 0 )
		return order;

	int start = startIndex;
	if( start < 0 )
		start = 0;
	if( start >= count )
		start %= count;

	order.reserve((size_t)count);
	for( int offset = 0; offset < count; ++offset )
		order.push_back((start + offset) % count);
	return order;
}

static vector<int> BuildReconnectOrder(const UiState& ui)
{
	vector<int> order;
	int count = (int)ui.enumeratedCameras.size();
	if( count <= 0 )
		return order;

	int matchedIndex = FindCameraIndexByIdentity(ui.enumeratedCameras, ui.activeIdentity);
	if( matchedIndex >= 0 )
		order.push_back(matchedIndex);

	int anchor = ui.lastKnownCameraIndex;
	if( anchor < 0 && matchedIndex >= 0 )
		anchor = matchedIndex;
	if( anchor < 0 )
		anchor = 0;
	if( anchor >= count )
		anchor %= count;

	for( int offset = 1; offset <= count; ++offset )
	{
		int index = (anchor + offset) % count;
		bool alreadyAdded = false;
		for( size_t i = 0; i < order.size(); ++i )
		{
			if( order[i] == index )
			{
				alreadyAdded = true;
				break;
			}
		}
		if( !alreadyAdded )
			order.push_back(index);
	}

	return order;
}

static vector<int> BuildReturnOrder(const UiState& ui)
{
	vector<int> order;
	int count = (int)ui.enumeratedCameras.size();
	if( count <= 0 )
		return order;

	int matchedIndex = FindCameraIndexByIdentity(ui.enumeratedCameras, ui.activeIdentity);
	if( matchedIndex >= 0 )
		order.push_back(matchedIndex);

	for( int index = 0; index < count; ++index )
	{
		bool alreadyAdded = false;
		for( size_t i = 0; i < order.size(); ++i )
		{
			if( order[i] == index )
			{
				alreadyAdded = true;
				break;
			}
		}
		if( !alreadyAdded )
			order.push_back(index);
	}

	return order;
}

static vector<int> BuildSwitchOrder(const UiState& ui)
{
	vector<int> order;
	int count = (int)ui.enumeratedCameras.size();
	if( count <= 1 )
		return order;

	int currentIndex = ui.highlightedCameraIndex;
	if( currentIndex < 0 )
		currentIndex = ui.lastKnownCameraIndex;
	if( currentIndex < 0 )
		currentIndex = 0;
	if( currentIndex >= count )
		currentIndex %= count;

	for( int offset = 1; offset < count; ++offset )
		order.push_back((currentIndex + offset) % count);
	return order;
}

static int GetDirectSelectionIndex(bool prevNumberDown[10], int cameraCount)
{
	const int keyCodes[10] = { KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0 };
	for( int i = 0; i < 10; ++i )
	{
		bool down = key[keyCodes[i]] != 0;
		bool pressed = down && !prevNumberDown[i];
		prevNumberDown[i] = down;
		if( pressed && i < cameraCount )
			return i;
	}
	return -1;
}

static bool TryOpenSequence(CameraPixels& cam, UiState& ui, const vector<int>& order, DWORD now, wstring& lastError)
{
	lastError.clear();
	if( !order.empty() )
		BeginCameraAttempt(ui, order[0]);
	for( size_t i = 0; i < order.size(); ++i )
	{
		if( OpenCameraAtIndex(cam, ui, order[i], now) )
			return true;
		lastError = cam.lastError();
	}
	return false;
}

static void RefreshEnumeratedCameras(UiState& ui, bool hasOpenCamera, DWORD now)
{
	ui.enumeratedCameras = CameraPixels::enumerate();
	ui.multipleCamerasAvailable = ui.enumeratedCameras.size() > 1;
	ui.lastEnumerationTick = now;

	int matchedIndex = FindCameraIndexByIdentity(ui.enumeratedCameras, ui.activeIdentity);
	if( matchedIndex >= 0 )
	{
		ui.openedCameraIndex = matchedIndex;
		ui.lastKnownCameraIndex = matchedIndex;
		ui.highlightedCameraIndex = matchedIndex;
		ui.currentCameraName = ui.enumeratedCameras[matchedIndex];
	}
	else if( !hasOpenCamera )
	{
		ui.openedCameraIndex = -1;
		if( ui.enumeratedCameras.empty() )
			ui.highlightedCameraIndex = -1;
	}
	else
	{
		ui.openedCameraIndex = -1;
	}

	if( !hasOpenCamera && ui.enumeratedCameras.empty() )
		ui.currentCameraName.clear();
}

static void DrawOverlayUi(BITMAP* buff, const UiState& ui, bool hasOpenCamera)
{
	const int left = 8;
	int y = 8;
	const int lineHeight = text_height(font);
	const int white = makecol(255, 255, 255);
	const int green = makecol(64, 255, 64);
	const int red = makecol(255, 0, 0);
	const int cyan = makecol(96, 255, 255);

	wstring header = L"Cameras";
	if( ui.enumeratedCameras.size() >= 2 )
	{
		wstringstream headerStream;
		headerStream << L"Cameras (switch with Tab / 1.." << min(10, (int)ui.enumeratedCameras.size()) << L")";
		header = headerStream.str();
	}
	DrawUiLine(buff, left, y, white, header);
	y += lineHeight;

	if( ui.enumeratedCameras.empty() )
	{
		DrawUiLine(buff, left, y, red, L"(none)");
		return;
	}

	for( int i = 0; i < (int)ui.enumeratedCameras.size(); ++i )
	{
		bool selected = i == ui.highlightedCameraIndex;
		int color = white;
		if( selected )
		{
			if( ui.awaitingFirstFrame )
				color = cyan;
			else if( hasOpenCamera && !ui.disconnected && i == ui.openedCameraIndex )
				color = green;
			else
				color = red;
		}

		wstringstream line;
		line << (selected ? L"> " : L"  ") << (i + 1) << L": " << ui.enumeratedCameras[i];
		DrawUiLine(buff, left, y, color, line.str());
		y += lineHeight;
	}
}

#undef main
int main()
{
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

	CameraPixels cam;
	CameraPixels::Frame cameraFrame;
	CameraPixels::Frame processedFrame;
	FeatureDetector fd;
	UiState ui;
	bool prevTabDown = false;
	bool prevNumberDown[10] = { false, false, false, false, false, false, false, false, false, false };

	DWORD now = NowMs();
	RefreshEnumeratedCameras(ui, cam.isOpen(), now);

	wstring openError;
	if( ui.enumeratedCameras.empty() )
	{
		ui.statusLine = L"No cameras available";
	}
	else if( TryOpenSequence(cam, ui, BuildReturnOrder(ui), now, openError) )
	{
		ui.statusLine = L"Opened camera: " + ui.currentCameraName;
	}
	else
	{
		ui.awaitingFirstFrame = false;
		ui.disconnected = true;
		ui.statusLine = BuildStatusWithError(L"Failed to open any camera", openError);
	}

	while(!key[KEY_ESC] && !exited)
	{
		now = NowMs();
		bool reenumerated = false;
		if( ui.lastEnumerationTick == 0 || now - ui.lastEnumerationTick >= 1000 )
		{
			RefreshEnumeratedCameras(ui, cam.isOpen(), now);
			reenumerated = true;
		}

		clear_to_color(buff, makecol(0, 0, 0));

		bool gotFrame = false;
		if( cam.isOpen() && cam.readBGRA(cameraFrame, 1000) )
		{
			gotFrame = true;
			ui.lastFrameTick = NowMs();
			ui.awaitingFirstFrame = false;
			if( ui.disconnected )
			{
				ui.disconnected = false;
				ui.statusLine = L"Camera resumed: " + ui.currentCameraName;
			}

			if( !ScaleBGRABoxAverageToFit(cameraFrame, SCREEN_W, SCREEN_H, processedFrame) )
				processedFrame = cameraFrame;

			fd.Process(processedFrame);
			int dstX = 0;
			int dstY = 0;
			int dstW = 0;
			int dstH = 0;
			ComputeFitRect(processedFrame.width, processedFrame.height, SCREEN_W, SCREEN_H, dstX, dstY, dstW, dstH);
			fd.ShowResult(buff, fd.output, 4, dstX, dstY, dstW, dstH);
		}

		int directSelectionIndex = GetDirectSelectionIndex(prevNumberDown, (int)ui.enumeratedCameras.size());
		if( directSelectionIndex >= 0 && (!cam.isOpen() || directSelectionIndex != ui.openedCameraIndex) )
		{
			wstring selectError;
			if( TryOpenSequence(cam, ui, BuildWrappedIndices((int)ui.enumeratedCameras.size(), directSelectionIndex), NowMs(), selectError) )
			{
				ui.statusLine = L"Switched camera: " + ui.currentCameraName;
			}
			else
			{
				ui.openedCameraIndex = -1;
				ui.highlightedCameraIndex = directSelectionIndex;
				ui.awaitingFirstFrame = false;
				ui.disconnected = true;
				ui.statusLine = BuildStatusWithError(L"Failed to switch cameras", selectError);
			}
		}

		bool tabDown = key[KEY_TAB] != 0;
		if( tabDown && !prevTabDown && ui.enumeratedCameras.size() >= 2 )
		{
			vector<int> switchOrder = BuildSwitchOrder(ui);
			wstring switchError;
			if( TryOpenSequence(cam, ui, switchOrder, NowMs(), switchError) )
			{
				ui.statusLine = L"Switched camera: " + ui.currentCameraName;
			}
			else
			{
				ui.openedCameraIndex = -1;
				ui.awaitingFirstFrame = false;
				ui.disconnected = true;
				ui.statusLine = BuildStatusWithError(L"Failed to switch cameras", switchError);
			}
		}
		prevTabDown = tabDown;

		now = NowMs();
		if( cam.isOpen() && !gotFrame && ui.lastFrameTick != 0 && now - ui.lastFrameTick >= 3000 )
		{
			ui.disconnected = true;
			vector<int> reconnectOrder = BuildReconnectOrder(ui);
			wstring reconnectError;
			wstring previousName = ui.currentCameraName;
			bool hadMatchedCamera = FindCameraIndexByIdentity(ui.enumeratedCameras, ui.activeIdentity) >= 0;
			if( TryOpenSequence(cam, ui, reconnectOrder, now, reconnectError) )
			{
				if( ui.currentCameraName == previousName )
					ui.statusLine = L"Reopened camera: " + ui.currentCameraName;
				else
					ui.statusLine = L"Switched to fallback camera: " + ui.currentCameraName;
			}
			else
			{
				ui.openedCameraIndex = -1;
				ui.awaitingFirstFrame = false;
				if( ui.enumeratedCameras.empty() )
					ui.statusLine = L"Camera disconnected: no cameras available";
				else if( hadMatchedCamera )
					ui.statusLine = BuildStatusWithError(L"Camera stream stalled and could not be reopened", reconnectError);
				else
					ui.statusLine = BuildStatusWithError(L"Active camera disappeared and fallback failed", reconnectError);
			}
		}

		if( !cam.isOpen() && reenumerated )
		{
			wstring reopenError;
			if( ui.enumeratedCameras.empty() )
			{
				ui.highlightedCameraIndex = -1;
				ui.statusLine = L"No cameras available";
			}
			else if( TryOpenSequence(cam, ui, BuildReturnOrder(ui), now, reopenError) )
			{
				ui.statusLine = L"Opened camera: " + ui.currentCameraName;
			}
			else
			{
				ui.awaitingFirstFrame = false;
				ui.disconnected = true;
				ui.statusLine = BuildStatusWithError(L"Failed to open any camera", reopenError);
			}
		}

		DrawOverlayUi(buff, ui, cam.isOpen());

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

