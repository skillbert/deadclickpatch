//this hook function doesn't have to unhook and rehook at every call
//this prevents race conditions when multiple threads are calling the same hooked function
//it comes with some serious drawbacks in return so its use is limited



//right now this only works if the hookfunction is reachable in a 32bit relative jump and the first 5 bytes of the hooked function are a whole number of instructions


#pragma once

#define WINVER         0x0600
#define _WIN32_WINDOWS 0x0600
#define _WIN32_WINNT   0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
using namespace std;


typedef BOOL(WINAPI PEEKMESSAGE)(LPMSG, HWND, UINT, UINT, UINT);

//hardcode to peekmessage, visual studio debugger/ide really doesn't like templates
//template<typename FN>
typedef PEEKMESSAGE FN;
class HardHook
{
	//buffer with the instruction from the fnc that we cut out+a jump to the original func body
	//should probably allocate a new page for this but w/e
	BYTE data[100];
	//address of hooked function
	LPBYTE func = NULL;
	//address of injected function
	LPBYTE hookFunc = NULL;
	int gapsize;
public:
	FN* invoke;
	inline HardHook() {
		FillMemory(data, sizeof(data), 0xCC);
		invoke = (FN*)(void*)data;
	}

	static int WriteJump64(LPBYTE addr, const void* targetAddr) {
		DWORD oldProtect;
		VirtualProtect((LPVOID)addr, 14, PAGE_EXECUTE_READWRITE, &oldProtect);

		*(addr + 0) = 0xFF;
		*(addr + 1) = 0x25;//64bit abs jump, address is at offset of instruction pointer
		*((LPDWORD)(addr + 2)) = 0;//offset of 0 (so at address is after current instruction)
		*((unsigned __int64*)(addr + 6)) = (__int64)targetAddr;//jump target address
		return 14;
	}

	static int WriteJumpRel32(LPBYTE addr,const void* targetAddr) {
		DWORD oldProtect;
		VirtualProtect(addr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

		DWORD jump = DWORD((INT_PTR)targetAddr - ((INT_PTR)addr + 5));
		
		*(addr+0) = 0xE9;//relative signed 32bit jump
		*(DWORD*)(addr + 1) = jump;//relative addr
		return 5;
	}

	inline bool Hook(FN funcin, FN hookFuncin) {
		func = (LPBYTE)funcin;
		hookFunc = (LPBYTE)hookFuncin;

		//number of bytes relocated from function head
		//should do some disassembling to find a number of bytes without breaking up an instruction
		int pos = 0;
		gapsize = 5;
		memcpy(data, funcin, gapsize);
		pos += gapsize;
		pos += WriteJump64(data+pos, func + gapsize);

		//enable execution on our buffer
		DWORD oldProtect;
		VirtualProtect(data, pos, PAGE_EXECUTE_READWRITE, &oldProtect);

		//write a jump to our hook
		//this has the assumption that our hook is in 32bit range
		WriteJumpRel32(func, hookFunc);
		return true;
	}

	inline bool Unhook() {
		memcpy(func, data, gapsize);
	}

};

