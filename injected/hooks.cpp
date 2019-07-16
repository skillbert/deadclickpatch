#include "obshooks.h"
#include "customhardhook.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <fstream>

#define _CRT_SECURE_NO_WARNINGS 1

//output to log file?
#define OUTFILE 0

//globals
ofstream ofs;
HINSTANCE hinstDLL;

//hooks
HookData hookSwapBuffers;
HardHook hardhookPeekmessage;


//predefines
bool inithooks();
BOOL WINAPI runPeekMessageA(LPMSG lpmsg, HWND hwnd, UINT wfiltermin, UINT wfiltermax, UINT remove);
BOOL WINAPI runSwapBuffers(HDC hdc);

//dll main
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID lpBlah)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hinstDLL = hinst;
		inithooks();
	}

	return TRUE;
}

bool inithooks()
{
	if (OUTFILE) {
		//find the absolute path to our dll because cwd is in the rs folder
		TCHAR dllpathchars[MAX_PATH];
		GetModuleFileName(hinstDLL, dllpathchars, MAX_PATH);
		string dllpath = string(dllpathchars);
		int index = dllpath.find_last_of("\\/");
		string debugfile = dllpath.substr(0, index + 1) + "debug.txt";
		ofs = ofstream(debugfile, std::ios_base::app);
	}

	//peekmessage is called from multiple threads so use our custom hook
	hardhookPeekmessage.Hook(PeekMessageA, runPeekMessageA);

	//use know and tested obs hook for buffer swap
	hookSwapBuffers.Hook((FARPROC)SwapBuffers, (FARPROC)runSwapBuffers);
	hookSwapBuffers.Rehook();

	return true;
}


std::mutex peekmutex;
int framenr = 0;
int lastkeydown = -1;
int lastkeyup = -1;
int lastkeydownid = 0;
BOOL WINAPI runPeekMessageA(LPMSG lpmsg, HWND hwnd, UINT wfiltermin, UINT wfiltermax, UINT remove) {
	std::lock_guard<std::mutex> lock(peekmutex);

	bool allow = true;
	//look at the next message without removing it from the queue
	BOOL r = hardhookPeekmessage.invoke(lpmsg, hwnd, wfiltermin, wfiltermax, 0);
	if (r) {
		bool iskeydown = lpmsg->message == WM_KEYDOWN || lpmsg->message == WM_SYSKEYDOWN;
		bool iskeyup = lpmsg->message == WM_KEYUP || lpmsg->message == WM_SYSKEYUP;
		bool issyskey = lpmsg->message == WM_SYSKEYDOWN || lpmsg->message == WM_SYSKEYUP;

		//modifier keys only at start of frame
		if (issyskey && (lastkeydown == framenr || lastkeyup == framenr)) { allow = false; }
		//only allow keydowns in same frame if virtual keycode is higher than previous key
		if (iskeydown && lastkeydown == framenr &&lpmsg->wParam < lastkeydownid) { allow = false; }
		//dont allow any key ups in same frame as the key down
		if (iskeyup && lastkeydown == framenr) { allow = false; }

		if (!allow) {
			//tell rs that there are no more messages
			r = 0;
		}
		else {
			//remove the message from the queue
			r = hardhookPeekmessage.invoke(lpmsg, hwnd, wfiltermin, wfiltermax, remove);

			//update our state
			if (iskeydown && !issyskey) {
				lastkeydown = framenr;
				lastkeydownid = lpmsg->wParam;
			}
			if (iskeyup && !issyskey) {
				lastkeyup = framenr;
			}
		}
	}

	if (r || !allow) {
		if (OUTFILE) {
			ofs << "peekmessage n:" << framenr << " threadid:" << std::this_thread::get_id() << (allow ? "" : " BLOCKED") << " mes:" << lpmsg->message << " lparam:" << lpmsg->lParam << " wparam:" << lpmsg->wParam << std::endl;
		}
	}
	return r;
}

BOOL WINAPI runSwapBuffers(HDC hdc) {
	hookSwapBuffers.Unhook();
	BOOL r = SwapBuffers(hdc);
	hookSwapBuffers.Rehook();
	framenr++;
	return r;
}

