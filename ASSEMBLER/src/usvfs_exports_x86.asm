.686
.MODEL FLAT
OPTION CASEMAP:NONE

; External Impl functions (extern "C" WINAPI in bridge)
; stdcall functions on x86 are decorated with _Name@Size
EXTERN _usvfsAsmBlacklistExecutableImpl@4:PROC
EXTERN _usvfsAsmClearExecutableBlacklistImpl@0:PROC
EXTERN _usvfsAsmClearLibraryForceLoadsImpl@0:PROC
EXTERN _usvfsAsmClearVirtualMappingsImpl@0:PROC
EXTERN _usvfsAsmConnectVFSImpl@4:PROC
EXTERN _usvfsAsmCreateHookContextImpl:PROC ; This was __cdecl in bridge? Let's check.
EXTERN _usvfsAsmCreateMiniDumpImpl@12:PROC
EXTERN _usvfsAsmCreateProcessHookedImpl@40:PROC
EXTERN _usvfsAsmCreateVFSDumpImpl@8:PROC
EXTERN _usvfsAsmCreateVFSImpl@4:PROC
EXTERN _usvfsAsmDisconnectVFSImpl@0:PROC
EXTERN _usvfsAsmForceLoadLibraryImpl@8:PROC
EXTERN _usvfsAsmGetCurrentVFSNameImpl@8:PROC
EXTERN _usvfsAsmGetLogMessagesImpl@12:PROC
EXTERN _usvfsAsmGetVFSProcessListImpl@8:PROC
EXTERN _usvfsAsmGetVFSProcessList2Impl@8:PROC
EXTERN _usvfsAsmInitLoggingImpl@4:PROC
EXTERN _usvfsAsmInitHooksImpl:PROC ; __cdecl
EXTERN _usvfsAsmPrintDebugInfoImpl@0:PROC
EXTERN _usvfsAsmUSVFSInitParametersImpl@24:PROC
EXTERN _usvfsAsmUSVFSUpdateParamsImpl@8:PROC
EXTERN _usvfsAsmUSVFSVersionStringImpl@0:PROC
EXTERN _usvfsAsmUsvfsConnectVFSImpl@4:PROC
EXTERN _usvfsAsmUsvfsCreateHookContextImpl@8:PROC
EXTERN _usvfsAsmUsvfsCreateVFSImpl@4:PROC
EXTERN _usvfsAsmUsvfsUpdateParametersImpl@4:PROC
EXTERN _usvfsAsmVirtualLinkDirectoryStaticImpl@12:PROC
EXTERN _usvfsAsmVirtualLinkFileImpl@12:PROC

; v2.5.x extensions
EXTERN _usvfsAsmUsvfsAddSkipDirectoryImpl@4:PROC
EXTERN _usvfsAsmUsvfsAddSkipFileSuffixImpl@4:PROC
EXTERN _usvfsAsmUsvfsClearSkipDirectoriesImpl@0:PROC
EXTERN _usvfsAsmUsvfsClearSkipFileSuffixesImpl@0:PROC

.CODE

; CreateHookContext and InitHooks were __cdecl or special?
; From bridge: 
; extern "C" usvfs::HookContext* __cdecl _usvfsAsmCreateHookContextImpl(LPVOID parameters, size_t size)
; extern "C" void __cdecl _usvfsAsmInitHooksImpl(LPVOID parameters, size_t)

PUBLIC _CreateHookContext
_CreateHookContext PROC
    jmp _usvfsAsmCreateHookContextImpl
_CreateHookContext ENDP

PUBLIC _InitHooks
_InitHooks PROC
    jmp _usvfsAsmInitHooksImpl
_InitHooks ENDP

; Standard API (stdcall wrappers)
; Note: The .def file aliases these to the undecorated names or decorated ones as needed.
; We use the decorated names here for public symbols to match the bridge linkage.

PUBLIC _BlacklistExecutable@4
_BlacklistExecutable@4 PROC
    jmp _usvfsAsmBlacklistExecutableImpl@4
_BlacklistExecutable@4 ENDP

PUBLIC _ClearExecutableBlacklist@0
_ClearExecutableBlacklist@0 PROC
    jmp _usvfsAsmClearExecutableBlacklistImpl@0
_ClearExecutableBlacklist@0 ENDP

PUBLIC _ClearLibraryForceLoads@0
_ClearLibraryForceLoads@0 PROC
    jmp _usvfsAsmClearLibraryForceLoadsImpl@0
_ClearLibraryForceLoads@0 ENDP

PUBLIC _ClearVirtualMappings@0
_ClearVirtualMappings@0 PROC
    jmp _usvfsAsmClearVirtualMappingsImpl@0
_ClearVirtualMappings@0 ENDP

PUBLIC _ConnectVFS@4
_ConnectVFS@4 PROC
    jmp _usvfsAsmConnectVFSImpl@4
_ConnectVFS@4 ENDP

PUBLIC _CreateMiniDump@12
_CreateMiniDump@12 PROC
    jmp _usvfsAsmCreateMiniDumpImpl@12
_CreateMiniDump@12 ENDP

PUBLIC _CreateProcessHooked@40
_CreateProcessHooked@40 PROC
    jmp _usvfsAsmCreateProcessHookedImpl@40
_CreateProcessHooked@40 ENDP

PUBLIC _CreateVFS@4
_CreateVFS@4 PROC
    jmp _usvfsAsmCreateVFSImpl@4
_CreateVFS@4 ENDP

PUBLIC _CreateVFSDump@8
_CreateVFSDump@8 PROC
    jmp _usvfsAsmCreateVFSDumpImpl@8
_CreateVFSDump@8 ENDP

PUBLIC _DisconnectVFS@0
_DisconnectVFS@0 PROC
    jmp _usvfsAsmDisconnectVFSImpl@0
_DisconnectVFS@0 ENDP

PUBLIC _ForceLoadLibrary@8
_ForceLoadLibrary@8 PROC
    jmp _usvfsAsmForceLoadLibraryImpl@8
_ForceLoadLibrary@8 ENDP

PUBLIC _GetCurrentVFSName@8
_GetCurrentVFSName@8 PROC
    jmp _usvfsAsmGetCurrentVFSNameImpl@8
_GetCurrentVFSName@8 ENDP

PUBLIC _GetLogMessages@12
_GetLogMessages@12 PROC
    jmp _usvfsAsmGetLogMessagesImpl@12
_GetLogMessages@12 ENDP

PUBLIC _GetVFSProcessList@8
_GetVFSProcessList@8 PROC
    jmp _usvfsAsmGetVFSProcessListImpl@8
_GetVFSProcessList@8 ENDP

PUBLIC _GetVFSProcessList2@8
_GetVFSProcessList2@8 PROC
    jmp _usvfsAsmGetVFSProcessList2Impl@8
_GetVFSProcessList2@8 ENDP

PUBLIC _InitLogging@4
_InitLogging@4 PROC
    jmp _usvfsAsmInitLoggingImpl@4
_InitLogging@4 ENDP

PUBLIC _PrintDebugInfo@0
_PrintDebugInfo@0 PROC
    jmp _usvfsAsmPrintDebugInfoImpl@0
_PrintDebugInfo@0 ENDP

PUBLIC _USVFSInitParameters@24
_USVFSInitParameters@24 PROC
    jmp _usvfsAsmUSVFSInitParametersImpl@24
_USVFSInitParameters@24 ENDP

PUBLIC _USVFSUpdateParams@8
_USVFSUpdateParams@8 PROC
    jmp _usvfsAsmUSVFSUpdateParamsImpl@8
_USVFSUpdateParams@8 ENDP

PUBLIC _USVFSVersionString@0
_USVFSVersionString@0 PROC
    jmp _usvfsAsmUSVFSVersionStringImpl@0
_USVFSVersionString@0 ENDP

PUBLIC _VirtualLinkDirectoryStatic@12
_VirtualLinkDirectoryStatic@12 PROC
    jmp _usvfsAsmVirtualLinkDirectoryStaticImpl@12
_VirtualLinkDirectoryStatic@12 ENDP

PUBLIC _VirtualLinkFile@12
_VirtualLinkFile@12 PROC
    jmp _usvfsAsmVirtualLinkFileImpl@12
_VirtualLinkFile@12 ENDP

PUBLIC _usvfsConnectVFS@4
_usvfsConnectVFS@4 PROC
    jmp _usvfsAsmUsvfsConnectVFSImpl@4
_usvfsConnectVFS@4 ENDP

PUBLIC _usvfsCreateHookContext@8
_usvfsCreateHookContext@8 PROC
    jmp _usvfsAsmUsvfsCreateHookContextImpl@8
_usvfsCreateHookContext@8 ENDP

PUBLIC _usvfsCreateVFS@4
_usvfsCreateVFS@4 PROC
    jmp _usvfsAsmUsvfsCreateVFSImpl@4
_usvfsCreateVFS@4 ENDP

PUBLIC _usvfsUpdateParameters@4
_usvfsUpdateParameters@4 PROC
    jmp _usvfsAsmUsvfsUpdateParametersImpl@4
_usvfsUpdateParameters@4 ENDP

; v2.5.x extensions
PUBLIC _usvfsAddSkipDirectory@4
_usvfsAddSkipDirectory@4 PROC
    jmp _usvfsAsmUsvfsAddSkipDirectoryImpl@4
_usvfsAddSkipDirectory@4 ENDP

PUBLIC _usvfsAddSkipFileSuffix@4
_usvfsAddSkipFileSuffix@4 PROC
    jmp _usvfsAsmUsvfsAddSkipFileSuffixImpl@4
_usvfsAddSkipFileSuffix@4 ENDP

PUBLIC _usvfsClearSkipDirectories@0
_usvfsClearSkipDirectories@0 PROC
    jmp _usvfsAsmUsvfsClearSkipDirectoriesImpl@0
_usvfsClearSkipDirectories@0 ENDP

PUBLIC _usvfsClearSkipFileSuffixes@0
_usvfsClearSkipFileSuffixes@0 PROC
    jmp _usvfsAsmUsvfsClearSkipFileSuffixesImpl@0
_usvfsClearSkipFileSuffixes@0 ENDP

; v2.5.x usvfs-prefixed aliases for core API
PUBLIC _usvfsDisconnectVFS@0
_usvfsDisconnectVFS@0 PROC
    jmp _DisconnectVFS@0
_usvfsDisconnectVFS@0 ENDP

PUBLIC _usvfsVirtualLinkDirectoryStatic@12
_usvfsVirtualLinkDirectoryStatic@12 PROC
    jmp _VirtualLinkDirectoryStatic@12
_usvfsVirtualLinkDirectoryStatic@12 ENDP

PUBLIC _usvfsCreateMiniDump@12
_usvfsCreateMiniDump@12 PROC
    jmp _CreateMiniDump@12
_usvfsCreateMiniDump@12 ENDP

PUBLIC _usvfsClearVirtualMappings@0
_usvfsClearVirtualMappings@0 PROC
    jmp _ClearVirtualMappings@0
_usvfsClearVirtualMappings@0 ENDP

PUBLIC _usvfsVirtualLinkFile@12
_usvfsVirtualLinkFile@12 PROC
    jmp _VirtualLinkFile@12
_usvfsVirtualLinkFile@12 ENDP

PUBLIC _usvfsInitLogging@4
_usvfsInitLogging@4 PROC
    jmp _InitLogging@4
_usvfsInitLogging@4 ENDP

PUBLIC _usvfsPrintDebugInfo@0
_usvfsPrintDebugInfo@0 PROC
    jmp _PrintDebugInfo@0
_usvfsPrintDebugInfo@0 ENDP

PUBLIC _usvfsVersionString@0
_usvfsVersionString@0 PROC
    jmp _USVFSVersionString@0
_usvfsVersionString@0 ENDP

PUBLIC _usvfsGetLogMessages@12
_usvfsGetLogMessages@12 PROC
    jmp _GetLogMessages@12
_usvfsGetLogMessages@12 ENDP

PUBLIC _usvfsCreateVFSDump@8
_usvfsCreateVFSDump@8 PROC
    jmp _CreateVFSDump@8
_usvfsCreateVFSDump@8 ENDP

PUBLIC _usvfsGetVFSProcessList@8
_usvfsGetVFSProcessList@8 PROC
    jmp _GetVFSProcessList@8
_usvfsGetVFSProcessList@8 ENDP

PUBLIC _usvfsGetVFSProcessList2@8
_usvfsGetVFSProcessList2@8 PROC
    jmp _GetVFSProcessList2@8
_usvfsGetVFSProcessList2@8 ENDP

PUBLIC _usvfsGetCurrentVFSName@8
_usvfsGetCurrentVFSName@8 PROC
    jmp _GetCurrentVFSName@8
_usvfsGetCurrentVFSName@8 ENDP

PUBLIC _usvfsBlacklistExecutable@4
_usvfsBlacklistExecutable@4 PROC
    jmp _BlacklistExecutable@4
_usvfsBlacklistExecutable@4 ENDP

PUBLIC _usvfsClearExecutableBlacklist@0
_usvfsClearExecutableBlacklist@0 PROC
    jmp _ClearExecutableBlacklist@0
_usvfsClearExecutableBlacklist@0 ENDP

PUBLIC _usvfsForceLoadLibrary@8
_usvfsForceLoadLibrary@8 PROC
    jmp _ForceLoadLibrary@8
_usvfsForceLoadLibrary@8 ENDP

PUBLIC _usvfsClearLibraryForceLoads@0
_usvfsClearLibraryForceLoads@0 PROC
    jmp _ClearLibraryForceLoads@0
_usvfsClearLibraryForceLoads@0 ENDP

PUBLIC _usvfsCreateProcessHooked@40
_usvfsCreateProcessHooked@40 PROC
    jmp _CreateProcessHooked@40
_usvfsCreateProcessHooked@40 ENDP

END
