

#include "alleg.h"

#include <base.h>
using namespace base;
using namespace std;

#include "sendkey.h"


struct FindByTitleData {
	std::wstring needle;
	HWND found = nullptr;
};


static LPARAM MakeKeyLParam(UINT vk, bool keyUp) {
	UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
	LPARAM lParam = 1;                 // repeat count
	lParam |= (sc << 16);              // scan code
	if( keyUp ) {
		lParam |= (1u << 30);          // previous key state
		lParam |= (1u << 31);          // transition state
	}
	return lParam;
}

void SendVk_WMKEY(HWND hwnd, UINT vk)
{
	LRESULT res;
	
	res = SendMessageW(hwnd, WM_KEYDOWN, vk, MakeKeyLParam(vk, false));
	printf("WM_KEYDOWN -> %d\n", res);

	res = SendMessageW(hwnd, WM_KEYUP, vk, MakeKeyLParam(vk, true));
	printf("WM_KEYUP -> %d\n", res);
}

bool FocusWindow(HWND hwnd) {
	if( !IsWindow(hwnd) ) return false;

	ShowWindow(hwnd, SW_RESTORE);

	// Sometimes needed due to foreground restrictions:
	DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
	DWORD tgtThread = GetWindowThreadProcessId(hwnd, nullptr);
	if( fgThread != tgtThread ) {
		AttachThreadInput(fgThread, tgtThread, TRUE);
		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
		AttachThreadInput(fgThread, tgtThread, FALSE);
	}
	else {
		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
	}
	return GetForegroundWindow() == hwnd;
}

void SendUnicodeText_SendInput(const std::wstring& text) {
	for( wchar_t ch : text ) {
		INPUT in[2]{};
		in[0].type = INPUT_KEYBOARD;
		in[0].ki.wScan = ch;
		in[0].ki.dwFlags = KEYEVENTF_UNICODE;

		in[1] = in[0];
		in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

		SendInput(2, in, sizeof(INPUT));
	}
}

void SendVk_SendInput(WORD vk) {
	INPUT in[2]{};
	in[0].type = INPUT_KEYBOARD;
	in[0].ki.wVk = vk;

	in[1] = in[0];
	in[1].ki.dwFlags = KEYEVENTF_KEYUP;

	SendInput(2, in, sizeof(INPUT));
}





static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	auto* data = reinterpret_cast<FindByTitleData*>(lParam);

	if( !IsWindowVisible(hwnd) ) return TRUE;

	wchar_t title[512];
	int len = GetWindowTextW(hwnd, title, (int)std::size(title));
	if( len <= 0 ) return TRUE;

	std::wstring t(title, title + len);
	if( t.find(L"Engine") != std::wstring::npos )
		return TRUE;

	if( t.find(L"camera") != std::wstring::npos )
		return TRUE;

	if( t.find(data->needle) != std::wstring::npos ) {
		printf("Found: %ls\n", title);
		data->found = hwnd;
		return FALSE; // stop enumeration
	}
	return TRUE;
}

HWND FindTopLevelWindowByTitleContains(const std::wstring& needle)
{
	FindByTitleData data{ needle };
	EnumWindows((WNDENUMPROC)EnumWindowsProc, (LPARAM)&data);
	return data.found;
}

void SendKeyToWindow(const std::wstring& needle, WORD vk)
{
	HWND hWnd = FindTopLevelWindowByTitleContains(needle);

	printf("Window = %p\n", hWnd);
	if( hWnd && FocusWindow(hWnd) )
		SendUnicodeText_SendInput(L"u");
}
