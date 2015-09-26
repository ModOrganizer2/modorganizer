/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "inject.h"
/*#if defined UNICODE && !defined _UNICODE
#define _UNICODE 1
#endif*/
#include <wchar.h>
#include <cstdio>
#include <tchar.h>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include "windows_error.h"
#include "error_report.h"

namespace MOShared {


struct TParameters {
  char dllname[MAX_PATH];
  wchar_t profileName[101];
  char initstr[5];
  int  logLevel;
};


typedef HMODULE (WINAPI *TLoadLibraryType)(LPCTSTR);
typedef FARPROC (WINAPI *TGetProcAddressType)(HMODULE, LPCSTR);



void injectDLL(HANDLE processHandle, HANDLE threadHandle, const std::string &dllname, const std::wstring &profileName, int logLevel)
{
  // prepare parameters that are to be passed to the injected dll
  TParameters parameters;
  memset(&parameters, '\0', sizeof(TParameters));
  strncpy(parameters.dllname, dllname.c_str(), MAX_PATH - 1);
  wcsncpy(parameters.profileName, profileName.c_str(), 100);
  _snprintf(parameters.initstr, 5, "Init"); //this is the name of thie initialisation function we want to call in the target process

  HMODULE k32mod = ::LoadLibrary(__TEXT("kernel32.dll"));
  TLoadLibraryType loadLibraryFunc = nullptr;
  TGetProcAddressType getProcAddressFunc = nullptr;
  // ansi binaries
  if (k32mod != nullptr) {
    loadLibraryFunc = reinterpret_cast<TLoadLibraryType>(::GetProcAddress(k32mod, "LoadLibraryA"));
    getProcAddressFunc = reinterpret_cast<TGetProcAddressType>(::GetProcAddress(k32mod, "GetProcAddress"));
    if ((loadLibraryFunc == nullptr) || (getProcAddressFunc == nullptr)) {
      throw windows_error("failed to determine address for required functions");
    }
  } else {
    throw windows_error("kernel32.dll not loaded??");
  }

  // allocate memory in the target process and write the parameter-block there
  LPVOID remoteMem = ::VirtualAllocEx(processHandle, nullptr, sizeof(TParameters), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (remoteMem == nullptr) {
    throw windows_error("failed to allocate memory in target process");
  }
  SIZE_T written;
  if (!::WriteProcessMemory(processHandle, remoteMem, &parameters, sizeof(TParameters), &written) ||
      (written != sizeof(TParameters))) {
    throw windows_error("failed to write parameters to target process");
  }

  // now for the interesting part: write a stub into the target process that is run before any code of the original binary. This code will load
  // our injected dll into the process and run its Init-function which takes the profile name as its parameter
  // obviously this code is ultra-hacky

  // construct the stub in beautiful assembler with placeholders
  BYTE stubLocal[] = { 0x60,                         // PUSHAD
                       0xB8, 0xBA, 0xAD, 0xF0, 0x0D, // MOV EAX, imm32 (LoadLibrary)
                       0x68, 0xBA, 0xAD, 0xF0, 0x0D, // PUSH imm32 (dllname)
                       0xFF, 0xD0,                   // CALL EAX (=LoadLibrary, leaves module handle of our dll in eax)
                       0x68, 0xBA, 0xAD, 0xF0, 0x0D, // PUSH imm32 ("Init")
                       0x50,                         // PUSH EAX
                       0xB8, 0xBA, 0xAD, 0xF0, 0x0D, // MOVE EAX, imm32 (GetProcAddress)
                       0xFF, 0xD0,                   // CALL EAX (=GetProcAddress, leaves address of init function in eax)
                       0x68, 0xBA, 0xAD, 0xF0, 0x0D, // PUSH imm32 (profile name)
                       0x68, 0xBA, 0xAD, 0xF0, 0x0D, // PUSH imm32 (log level)
                       0xFF, 0xD0,                   // CALL EAX (=InitFunction)
                       0x58,                         // POP EAX (init function is defined cdecl)
                       0x58,                         // POP EAX (init function is defined cdecl)
                       0x61,                         // POPAD
                       0xE9, 0xBA, 0xAD, 0xF0, 0x0D  // JMP near, relative (=original entry point)
                    };

  // reserve memory for the stub
  PBYTE stubRemote = reinterpret_cast<PBYTE>(::VirtualAllocEx(processHandle, nullptr, sizeof(stubLocal), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  if (stubRemote == nullptr) {
    throw windows_error("failed to allocate memory for stub");
  }
  TParameters *remoteParams = reinterpret_cast<TParameters*>(remoteMem);

  // dizzy yet? we still have to calculate the entry point as an address relative to our stub
  ULONG entryPoint = 0;

  // not implemented on 64 bit systems
#ifdef _X86_
  CONTEXT threadContext;
  threadContext.ContextFlags = CONTEXT_CONTROL;
  if (::GetThreadContext(threadHandle, &threadContext) == 0) {
    throw windows_error("failed to access thread context. Please note that Mod Organizer does not support 64bit binaries!");
  } else {
    entryPoint = threadContext.Eip - (reinterpret_cast<ULONG>(stubRemote) + sizeof(stubLocal));
  }
  // now replace each baadf00d by the correct value. The pointers need to be the pointers in the REMOTE memory of course, that's why we copied them there
  *(PULONG)&stubLocal[2]  = reinterpret_cast<ULONG>(*loadLibraryFunc);
  *(PULONG)&stubLocal[7]  = reinterpret_cast<ULONG>(remoteParams->dllname);
  *(PULONG)&stubLocal[14] = reinterpret_cast<ULONG>(remoteParams->initstr);
  *(PULONG)&stubLocal[20] = reinterpret_cast<ULONG>(*getProcAddressFunc);
  *(PULONG)&stubLocal[27] = reinterpret_cast<ULONG>(remoteParams->profileName);
  *(PULONG)&stubLocal[32] = logLevel;
  *(PULONG)&stubLocal[42] = entryPoint;
  // almost there. copy stub to target process
  if (!::WriteProcessMemory(processHandle, stubRemote, reinterpret_cast<LPCVOID>(stubLocal), sizeof(stubLocal), &written) ||
      (written != sizeof(stubLocal))) {
    throw windows_error("failed to write stub to target process");
  }

  // finally, make the stub the new next thing for the thread to execute
  threadContext.Eip = (ULONG)stubRemote;
  if (::SetThreadContext(threadHandle, &threadContext) == 0) {
    throw windows_error("failed to overwrite thread context");
  }
#endif
}

} // namespace MOShared
