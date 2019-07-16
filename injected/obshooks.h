
//stripped down version of obs hooks
//some old version, idk where i got the exact code anymore


/********************************************************************************
Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#pragma once

#define WINVER         0x0600
#define _WIN32_WINDOWS 0x0600
#define _WIN32_WINNT   0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#define PSAPI_VERSION 1
#include <psapi.h>

#pragma intrinsic(memcpy, memset, memcmp)

#include <xmmintrin.h>
#include <emmintrin.h>

#include <objbase.h>

#include <string>
#include <sstream>
#include <fstream>
using namespace std;


#ifdef _WIN64
typedef unsigned __int64 UPARAM;
#else
typedef unsigned long UPARAM;
#endif

bool inithooks();

class HookData
{
	BYTE data[14];
	FARPROC func = NULL;
	FARPROC hookFunc = NULL;
	bool bHooked = false;
	bool b64bitJump = false;
	bool bAttemptedBounce = false;
	LPVOID bounceAddress;

public:
	inline HookData() {}

	inline bool Hook(FARPROC funcIn, FARPROC hookFuncIn)
	{
		if (bHooked)
		{
			if (funcIn == func)
			{
				if (hookFunc != hookFuncIn)
				{
					hookFunc = hookFuncIn;
					Rehook();
					return true;
				}
			}

			Unhook();
		}

		func = funcIn;
		hookFunc = hookFuncIn;

		DWORD oldProtect;
		if (!VirtualProtect((LPVOID)func, 14, PAGE_EXECUTE_READWRITE, &oldProtect))
			return false;

		memcpy(data, (const void*)func, 14);
		//VirtualProtect((LPVOID)func, 14, oldProtect, &oldProtect);

		return true;
	}

	inline void Rehook(bool bForce = false)
	{
		if ((!bForce && bHooked) || !func)
			return;

		UPARAM startAddr = UPARAM(func);
		UPARAM targetAddr = UPARAM(hookFunc);
		ULONG64 offset, diff;

		offset = targetAddr - (startAddr + 5);

		// we could be loaded above or below the target function
		if (startAddr + 5 > targetAddr)
			diff = startAddr + 5 - targetAddr;
		else
			diff = targetAddr - startAddr + 5;

#ifdef _WIN64
		// for 64 bit, try to use a shorter instruction sequence if we're too far apart, or we
		// risk overwriting other function prologues due to the 64 bit jump opcode length
		if (diff > 0x7fff0000 && !bAttemptedBounce)
		{
			MEMORY_BASIC_INFORMATION mem;

			// if we fail we don't want to continuously search memory every other call
			bAttemptedBounce = true;

			if (VirtualQueryEx(GetCurrentProcess(), (LPCVOID)startAddr, &mem, sizeof(mem)))
			{
				int i, pagesize;
				ULONG64 address;
				SYSTEM_INFO systemInfo;

				GetSystemInfo(&systemInfo);
				pagesize = systemInfo.dwAllocationGranularity;

				// try to allocate a page somewhere below the target
				for (i = 0, address = (ULONG64)mem.AllocationBase - pagesize; i < 256; i++, address -= pagesize)
				{
					bounceAddress = VirtualAlloc((LPVOID)address, pagesize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
					if (bounceAddress)
						break;
				}

				// if that failed, let's try above
				if (!bounceAddress)
				{
					for (i = 0, address = (ULONG64)mem.AllocationBase + mem.RegionSize + pagesize; i < 256; i++, address += pagesize)
					{
						bounceAddress = VirtualAlloc((LPVOID)address, pagesize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
						if (bounceAddress)
							break;
					}
				}

				if (bounceAddress)
				{
					// we found some space, let's try to put the 64 bit jump code there and fix up values for the original hook
					ULONG64 newdiff;

					if (startAddr + 5 > (UPARAM)bounceAddress)
						newdiff = startAddr + 5 - (UPARAM)bounceAddress;
					else
						newdiff = (UPARAM)bounceAddress - startAddr + 5;

					// first, see if we can reach it with a 32 bit jump
					if (newdiff <= 0x7fff0000)
					{
						// we can! update values so the shorter hook is written below
						FillMemory(bounceAddress, pagesize, 0xCC);

						// write new jmp
						LPBYTE addrData = (LPBYTE)bounceAddress;
						*(addrData++) = 0xFF;
						*(addrData++) = 0x25;
						*((LPDWORD)(addrData)) = 0;
						*((unsigned __int64*)(addrData + 4)) = targetAddr;

						targetAddr = (UPARAM)bounceAddress;
						offset = targetAddr - (startAddr + 5);
						diff = newdiff;
					}
				}
			}
		}
#endif

		DWORD oldProtect;

#ifdef _WIN64
		b64bitJump = (diff > 0x7fff0000);

		if (b64bitJump)
		{
			LPBYTE addrData = (LPBYTE)func;
			VirtualProtect((LPVOID)func, 14, PAGE_EXECUTE_READWRITE, &oldProtect);
			*(addrData++) = 0xFF;
			*(addrData++) = 0x25;
			*((LPDWORD)(addrData)) = 0;
			*((unsigned __int64*)(addrData + 4)) = targetAddr;
			//VirtualProtect((LPVOID)func, 14, oldProtect, &oldProtect);
		}
		else
#endif
		{
			VirtualProtect((LPVOID)func, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

			LPBYTE addrData = (LPBYTE)func;
			*addrData = 0xE9;
			*(DWORD*)(addrData + 1) = DWORD(offset);
			//VirtualProtect((LPVOID)func, 5, oldProtect, &oldProtect);
		}

		bHooked = true;
	}

	inline void Unhook()
	{
		if (!bHooked || !func)
			return;

		UINT count = b64bitJump ? 14 : 5;
		DWORD oldProtect;
		VirtualProtect((LPVOID)func, count, PAGE_EXECUTE_READWRITE, &oldProtect);
		memcpy((void*)func, data, count);
		//VirtualProtect((LPVOID)func, count, oldProtect, &oldProtect);

		bHooked = false;
	}
};

