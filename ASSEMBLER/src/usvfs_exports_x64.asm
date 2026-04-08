OPTION CASEMAP:NONE

EXTERN _usvfsAsmBlacklistExecutableImpl:PROC
EXTERN _usvfsAsmClearExecutableBlacklistImpl:PROC
EXTERN _usvfsAsmClearLibraryForceLoadsImpl:PROC
EXTERN _usvfsAsmClearVirtualMappingsImpl:PROC
EXTERN _usvfsAsmConnectVFSImpl:PROC
EXTERN _usvfsAsmCreateHookContextImpl:PROC
EXTERN _usvfsAsmCreateMiniDumpImpl:PROC
EXTERN _usvfsAsmCreateProcessHookedImpl:PROC
EXTERN _usvfsAsmCreateVFSDumpImpl:PROC
EXTERN _usvfsAsmCreateVFSImpl:PROC
EXTERN _usvfsAsmDisconnectVFSImpl:PROC
EXTERN _usvfsAsmForceLoadLibraryImpl:PROC
EXTERN _usvfsAsmGetCurrentVFSNameImpl:PROC
EXTERN _usvfsAsmGetLogMessagesImpl:PROC
EXTERN _usvfsAsmGetVFSProcessListImpl:PROC
EXTERN _usvfsAsmGetVFSProcessList2Impl:PROC
EXTERN _usvfsAsmInitLoggingImpl:PROC
EXTERN _usvfsAsmInitHooksImpl:PROC
EXTERN _usvfsAsmPrintDebugInfoImpl:PROC
EXTERN _usvfsAsmUSVFSInitParametersImpl:PROC
EXTERN _usvfsAsmUSVFSUpdateParamsImpl:PROC
EXTERN _usvfsAsmUSVFSVersionStringImpl:PROC
EXTERN _usvfsAsmUsvfsConnectVFSImpl:PROC
EXTERN _usvfsAsmUsvfsCreateHookContextImpl:PROC
EXTERN _usvfsAsmUsvfsCreateVFSImpl:PROC
EXTERN _usvfsAsmUsvfsUpdateParametersImpl:PROC
EXTERN _usvfsAsmVirtualLinkDirectoryStaticImpl:PROC
EXTERN _usvfsAsmVirtualLinkFileImpl:PROC
EXTERN _usvfsAsmUsvfsAddSkipDirectoryImpl:PROC
EXTERN _usvfsAsmUsvfsAddSkipFileSuffixImpl:PROC
EXTERN _usvfsAsmUsvfsClearSkipDirectoriesImpl:PROC
EXTERN _usvfsAsmUsvfsClearSkipFileSuffixesImpl:PROC


.CODE

PUBLIC BlacklistExecutable
BlacklistExecutable PROC
    jmp _usvfsAsmBlacklistExecutableImpl
BlacklistExecutable ENDP

PUBLIC ClearExecutableBlacklist
ClearExecutableBlacklist PROC
    jmp _usvfsAsmClearExecutableBlacklistImpl
ClearExecutableBlacklist ENDP

PUBLIC ClearLibraryForceLoads
ClearLibraryForceLoads PROC
    jmp _usvfsAsmClearLibraryForceLoadsImpl
ClearLibraryForceLoads ENDP

PUBLIC ClearVirtualMappings
ClearVirtualMappings PROC
    jmp _usvfsAsmClearVirtualMappingsImpl
ClearVirtualMappings ENDP

PUBLIC ConnectVFS
ConnectVFS PROC
    jmp _usvfsAsmConnectVFSImpl
ConnectVFS ENDP

PUBLIC CreateHookContext
CreateHookContext PROC
    jmp _usvfsAsmCreateHookContextImpl
CreateHookContext ENDP

PUBLIC CreateMiniDump
CreateMiniDump PROC
    jmp _usvfsAsmCreateMiniDumpImpl
CreateMiniDump ENDP

PUBLIC CreateProcessHooked
CreateProcessHooked PROC
    jmp _usvfsAsmCreateProcessHookedImpl
CreateProcessHooked ENDP

PUBLIC CreateVFSDump
CreateVFSDump PROC
    jmp _usvfsAsmCreateVFSDumpImpl
CreateVFSDump ENDP

PUBLIC CreateVFS
CreateVFS PROC
    jmp _usvfsAsmCreateVFSImpl
CreateVFS ENDP

PUBLIC DisconnectVFS
DisconnectVFS PROC
    jmp _usvfsAsmDisconnectVFSImpl
DisconnectVFS ENDP

PUBLIC ForceLoadLibrary
ForceLoadLibrary PROC
    jmp _usvfsAsmForceLoadLibraryImpl
ForceLoadLibrary ENDP

PUBLIC GetCurrentVFSName
GetCurrentVFSName PROC
    jmp _usvfsAsmGetCurrentVFSNameImpl
GetCurrentVFSName ENDP

PUBLIC GetLogMessages
GetLogMessages PROC
    jmp _usvfsAsmGetLogMessagesImpl
GetLogMessages ENDP

PUBLIC GetVFSProcessList
GetVFSProcessList PROC
    jmp _usvfsAsmGetVFSProcessListImpl
GetVFSProcessList ENDP

PUBLIC GetVFSProcessList2
GetVFSProcessList2 PROC
    jmp _usvfsAsmGetVFSProcessList2Impl
GetVFSProcessList2 ENDP

PUBLIC InitLogging
InitLogging PROC
    jmp _usvfsAsmInitLoggingImpl
InitLogging ENDP

PUBLIC InitHooks
InitHooks PROC
    jmp _usvfsAsmInitHooksImpl
InitHooks ENDP

PUBLIC PrintDebugInfo
PrintDebugInfo PROC
    jmp _usvfsAsmPrintDebugInfoImpl
PrintDebugInfo ENDP

PUBLIC USVFSInitParameters
USVFSInitParameters PROC
    jmp _usvfsAsmUSVFSInitParametersImpl
USVFSInitParameters ENDP

PUBLIC USVFSUpdateParams
USVFSUpdateParams PROC
    jmp _usvfsAsmUSVFSUpdateParamsImpl
USVFSUpdateParams ENDP

PUBLIC USVFSVersionString
USVFSVersionString PROC
    jmp _usvfsAsmUSVFSVersionStringImpl
USVFSVersionString ENDP

PUBLIC VirtualLinkDirectoryStatic
VirtualLinkDirectoryStatic PROC
    jmp _usvfsAsmVirtualLinkDirectoryStaticImpl
VirtualLinkDirectoryStatic ENDP

PUBLIC VirtualLinkFile
VirtualLinkFile PROC
    jmp _usvfsAsmVirtualLinkFileImpl
VirtualLinkFile ENDP

PUBLIC usvfsConnectVFS
usvfsConnectVFS PROC
    jmp _usvfsAsmUsvfsConnectVFSImpl
usvfsConnectVFS ENDP

PUBLIC usvfsCreateHookContext
usvfsCreateHookContext PROC
    jmp _usvfsAsmUsvfsCreateHookContextImpl
usvfsCreateHookContext ENDP

PUBLIC usvfsCreateVFS
usvfsCreateVFS PROC
    jmp _usvfsAsmUsvfsCreateVFSImpl
usvfsCreateVFS ENDP

PUBLIC usvfsUpdateParameters
usvfsUpdateParameters PROC
    jmp _usvfsAsmUsvfsUpdateParametersImpl
usvfsUpdateParameters ENDP

PUBLIC usvfsAddSkipDirectory
usvfsAddSkipDirectory PROC
    jmp _usvfsAsmUsvfsAddSkipDirectoryImpl
usvfsAddSkipDirectory ENDP

PUBLIC usvfsAddSkipFileSuffix
usvfsAddSkipFileSuffix PROC
    jmp _usvfsAsmUsvfsAddSkipFileSuffixImpl
usvfsAddSkipFileSuffix ENDP

PUBLIC usvfsClearSkipDirectories
usvfsClearSkipDirectories PROC
    jmp _usvfsAsmUsvfsClearSkipDirectoriesImpl
usvfsClearSkipDirectories ENDP

PUBLIC usvfsClearSkipFileSuffixes
usvfsClearSkipFileSuffixes PROC
    jmp _usvfsAsmUsvfsClearSkipFileSuffixesImpl
usvfsClearSkipFileSuffixes ENDP

PUBLIC usvfsBlacklistExecutable
usvfsBlacklistExecutable PROC
    jmp _usvfsAsmBlacklistExecutableImpl
usvfsBlacklistExecutable ENDP

PUBLIC usvfsClearExecutableBlacklist
usvfsClearExecutableBlacklist PROC
    jmp _usvfsAsmClearExecutableBlacklistImpl
usvfsClearExecutableBlacklist ENDP

PUBLIC usvfsClearLibraryForceLoads
usvfsClearLibraryForceLoads PROC
    jmp _usvfsAsmClearLibraryForceLoadsImpl
usvfsClearLibraryForceLoads ENDP

PUBLIC usvfsClearVirtualMappings
usvfsClearVirtualMappings PROC
    jmp _usvfsAsmClearVirtualMappingsImpl
usvfsClearVirtualMappings ENDP

PUBLIC usvfsCreateProcessHooked
usvfsCreateProcessHooked PROC
    jmp _usvfsAsmCreateProcessHookedImpl
usvfsCreateProcessHooked ENDP

PUBLIC usvfsCreateVFSDump
usvfsCreateVFSDump PROC
    jmp _usvfsAsmCreateVFSDumpImpl
usvfsCreateVFSDump ENDP

PUBLIC usvfsDisconnectVFS
usvfsDisconnectVFS PROC
    jmp _usvfsAsmDisconnectVFSImpl
usvfsDisconnectVFS ENDP

PUBLIC usvfsForceLoadLibrary
usvfsForceLoadLibrary PROC
    jmp _usvfsAsmForceLoadLibraryImpl
usvfsForceLoadLibrary ENDP

PUBLIC usvfsGetCurrentVFSName
usvfsGetCurrentVFSName PROC
    jmp _usvfsAsmGetCurrentVFSNameImpl
usvfsGetCurrentVFSName ENDP

PUBLIC usvfsGetLogMessages
usvfsGetLogMessages PROC
    jmp _usvfsAsmGetLogMessagesImpl
usvfsGetLogMessages ENDP

PUBLIC usvfsGetVFSProcessList
usvfsGetVFSProcessList PROC
    jmp _usvfsAsmGetVFSProcessListImpl
usvfsGetVFSProcessList ENDP

PUBLIC usvfsGetVFSProcessList2
usvfsGetVFSProcessList2 PROC
    jmp _usvfsAsmGetVFSProcessList2Impl
usvfsGetVFSProcessList2 ENDP

PUBLIC usvfsInitLogging
usvfsInitLogging PROC
    jmp _usvfsAsmInitLoggingImpl
usvfsInitLogging ENDP

PUBLIC usvfsPrintDebugInfo
usvfsPrintDebugInfo PROC
    jmp _usvfsAsmPrintDebugInfoImpl
usvfsPrintDebugInfo ENDP

PUBLIC usvfsCreateMiniDump
usvfsCreateMiniDump PROC
    jmp _usvfsAsmCreateMiniDumpImpl
usvfsCreateMiniDump ENDP

PUBLIC usvfsVersionString
usvfsVersionString PROC
    jmp _usvfsAsmUSVFSVersionStringImpl
usvfsVersionString ENDP

PUBLIC usvfsVirtualLinkDirectoryStatic
usvfsVirtualLinkDirectoryStatic PROC
    jmp _usvfsAsmVirtualLinkDirectoryStaticImpl
usvfsVirtualLinkDirectoryStatic ENDP

PUBLIC usvfsVirtualLinkFile
usvfsVirtualLinkFile PROC
    jmp _usvfsAsmVirtualLinkFileImpl
usvfsVirtualLinkFile ENDP

END
