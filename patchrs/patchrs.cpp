#pragma once
#include <Windows.h>
#include <string>
#include <tlhelp32.h>
#include <map>
#include <iostream>
using namespace std;

//predefines
DWORD GetProcessByName(wstring name);
inline std::wstring GetCurrentDir();
inline bool WINAPI InjectLibrary(HANDLE hProcess, std::wstring DLLName);

//main
int main() {
	DWORD pid = GetProcessByName(L"rs2client.exe");
	//DWORD pid = GetProcessByName(L"notepad.exe");
	if (!pid) {
		std::cout << "Failed to find rs process\n";
		system("pause");
		return 1;
	}
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
	if (!hProcess) {
		std::cout << "Couldn't open rs process\n";
		system("pause");
		return 1;
	}

	wstring currentDir = GetCurrentDir();
	wstring strDLL = currentDir + L"\\injected.dll";
	bool bSuccess = InjectLibrary(hProcess, strDLL) != 0;
	CloseHandle(hProcess);

	if (!bSuccess) {
		std::cout << "Failed to hook rs\n";
		system("pause");
		return 1;
	}

	std::cout << "Success!\n";
	system("pause");
	return 0;
}

DWORD GetProcessByName(wstring name)
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process;
	ZeroMemory(&process, sizeof(process));
	process.dwSize = sizeof(process);

	//Walk through all processes.
	DWORD pid = 0;
	bool first = true;
	while (first ? Process32First(snapshot, &process) : Process32Next(snapshot, &process)) {
		first = false;
		if (wstring(process.szExeFile) == name) {
			pid = process.th32ProcessID;
			break;
		}
	}
	CloseHandle(snapshot);
	return pid;
}

inline std::wstring GetCurrentDir() {
	WCHAR NPath[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, NPath);
	return std::wstring(NPath);
}

//OBS inject code

#ifdef _WIN64
typedef unsigned __int64 UPARAM;
#else
typedef unsigned long UPARAM;
#endif
typedef HANDLE(WINAPI *CRTPROC)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
typedef BOOL(WINAPI *WPMPROC)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
typedef LPVOID(WINAPI *VAEPROC)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
typedef BOOL(WINAPI *VFEPROC)(HANDLE, LPVOID, SIZE_T, DWORD);
typedef HANDLE(WINAPI *OPPROC)(DWORD, BOOL, DWORD);
inline bool WINAPI InjectLibrary(HANDLE hProcess, std::wstring DLLName)
{
	UPARAM procAddress;
	DWORD dwTemp, dwSize;
	LPVOID lpStr = NULL;
	BOOL bWorks;
	HANDLE hThread = NULL;
	SIZE_T writtenSize;
	bool bRet = false;

	if (!hProcess) return false;

	dwSize = DLLName.size() * sizeof(wchar_t) + 2;

	//The next part could be like 5 lines of code but i assume OBS did this to prevent bullshittery from naive antivirusses
	//--------------------------------------------------------

	int obfSize = 12;

	char pWPMStr[19], pCRTStr[19], pVAEStr[15], pVFEStr[14], pLLStr[13];
	memcpy(pWPMStr, "RvnrdPqmni|}Dmfegm", 19); //WriteProcessMemory with each character obfuscated
	memcpy(pCRTStr, "FvbgueQg`c{k]`yotp", 19); //CreateRemoteThread with each character obfuscated
	memcpy(pVAEStr, "WiqvpekGeddiHt", 15);     //VirtualAllocEx with each character obfuscated
	memcpy(pVFEStr, "Wiqvpek@{mnOu", 14);      //VirtualFreeEx with each character obfuscated
	memcpy(pLLStr, "MobfImethzr", 12);         //LoadLibrary with each character obfuscated

											   //Always use wide chars
	pLLStr[11] = 'W';
	pLLStr[12] = 0;

	obfSize += 6;
	for (int i = 0; i<obfSize; i++) pWPMStr[i] ^= i ^ 5;
	for (int i = 0; i<obfSize; i++) pCRTStr[i] ^= i ^ 5;

	obfSize -= 4;
	for (int i = 0; i<obfSize; i++) pVAEStr[i] ^= i ^ 1;

	obfSize -= 1;
	for (int i = 0; i<obfSize; i++) pVFEStr[i] ^= i ^ 1;

	obfSize -= 2;
	for (int i = 0; i<obfSize; i++) pLLStr[i] ^= i ^ 1;

	HMODULE hK32 = GetModuleHandle(TEXT("KERNEL32"));
	WPMPROC pWriteProcessMemory = (WPMPROC)GetProcAddress(hK32, pWPMStr);
	CRTPROC pCreateRemoteThread = (CRTPROC)GetProcAddress(hK32, pCRTStr);
	VAEPROC pVirtualAllocEx = (VAEPROC)GetProcAddress(hK32, pVAEStr);
	VFEPROC pVirtualFreeEx = (VFEPROC)GetProcAddress(hK32, pVFEStr);

	//--------------------------------------------------------

	lpStr = (LPVOID)(*pVirtualAllocEx)(hProcess, NULL, dwSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!lpStr) goto end;

	bWorks = (*pWriteProcessMemory)(hProcess, lpStr, DLLName.c_str(), dwSize, &writtenSize);
	if (!bWorks) goto end;

	procAddress = (UPARAM)GetProcAddress(hK32, pLLStr);
	if (!procAddress) goto end;

	hThread = (*pCreateRemoteThread)(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)procAddress, lpStr, 0, &dwTemp);
	if (!hThread) goto end;

	if (WaitForSingleObject(hThread, 2000) == WAIT_OBJECT_0)
	{
		DWORD dw;
		GetExitCodeThread(hThread, &dw);
		bRet = dw != 0;

		SetLastError(0);
	}

end:
	DWORD lastError;
	if (!bRet)
		lastError = GetLastError();

	if (hThread)
		CloseHandle(hThread);
	if (lpStr)
		(*pVirtualFreeEx)(hProcess, lpStr, 0, MEM_RELEASE);

	if (!bRet)
		SetLastError(lastError);

	return bRet;
}
