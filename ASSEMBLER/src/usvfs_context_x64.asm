OPTION CASEMAP:NONE

EXTERN usvfsAsmHookContextCtorImpl:PROC
EXTERN usvfsAsmHookContextDtorImpl:PROC
EXTERN usvfsAsmHookContextRemoveImpl:PROC
EXTERN usvfsAsmHookContextRegisterProcessImpl:PROC
EXTERN usvfsAsmHookContextUnregisterCurrentProcessImpl:PROC
EXTERN usvfsAsmHookContextBlacklistExecutableImpl:PROC
EXTERN usvfsAsmHookContextClearExecutableBlacklistImpl:PROC
EXTERN usvfsAsmHookContextExecutableBlacklistedImpl:PROC
EXTERN usvfsAsmHookContextForceLoadLibraryImpl:PROC
EXTERN usvfsAsmHookContextClearLibraryForceLoadsImpl:PROC
EXTERN usvfsAsmHookContextRegisterDelayedImpl:PROC
EXTERN usvfsAsmHookContextUnlockImpl:PROC
EXTERN usvfsAsmHookContextUnlockSharedImpl:PROC
EXTERN usvfsAsmHookContextRetrieveParametersImpl:PROC

EXTERN usvfsAsmHookManagerCtorImpl:PROC
EXTERN usvfsAsmHookManagerDtorImpl:PROC
EXTERN usvfsAsmHookManagerDetourImpl:PROC
EXTERN usvfsAsmHookManagerRemoveHookImpl:PROC
EXTERN usvfsAsmHookManagerLogStubIntImpl:PROC
EXTERN usvfsAsmHookManagerLogStubImpl:PROC
EXTERN usvfsAsmHookManagerInstallHookImpl:PROC
EXTERN usvfsAsmHookManagerInstallStubImpl:PROC
EXTERN usvfsAsmHookManagerInitHooksImpl:PROC
EXTERN usvfsAsmHookManagerRemoveHooksImpl:PROC

_usvfsAsmHookContextCtorImpl TEXTEQU <usvfsAsmHookContextCtorImpl>
_usvfsAsmHookContextDtorImpl TEXTEQU <usvfsAsmHookContextDtorImpl>
_usvfsAsmHookContextRemoveImpl TEXTEQU <usvfsAsmHookContextRemoveImpl>
_usvfsAsmHookContextRegisterProcessImpl TEXTEQU <usvfsAsmHookContextRegisterProcessImpl>
_usvfsAsmHookContextUnregisterCurrentProcessImpl TEXTEQU <usvfsAsmHookContextUnregisterCurrentProcessImpl>
_usvfsAsmHookContextBlacklistExecutableImpl TEXTEQU <usvfsAsmHookContextBlacklistExecutableImpl>
_usvfsAsmHookContextClearExecutableBlacklistImpl TEXTEQU <usvfsAsmHookContextClearExecutableBlacklistImpl>
_usvfsAsmHookContextExecutableBlacklistedImpl TEXTEQU <usvfsAsmHookContextExecutableBlacklistedImpl>
_usvfsAsmHookContextForceLoadLibraryImpl TEXTEQU <usvfsAsmHookContextForceLoadLibraryImpl>
_usvfsAsmHookContextClearLibraryForceLoadsImpl TEXTEQU <usvfsAsmHookContextClearLibraryForceLoadsImpl>
_usvfsAsmHookContextRegisterDelayedImpl TEXTEQU <usvfsAsmHookContextRegisterDelayedImpl>
_usvfsAsmHookContextUnlockImpl TEXTEQU <usvfsAsmHookContextUnlockImpl>
_usvfsAsmHookContextUnlockSharedImpl TEXTEQU <usvfsAsmHookContextUnlockSharedImpl>
_usvfsAsmHookContextRetrieveParametersImpl TEXTEQU <usvfsAsmHookContextRetrieveParametersImpl>

_usvfsAsmHookManagerCtorImpl TEXTEQU <usvfsAsmHookManagerCtorImpl>
_usvfsAsmHookManagerDtorImpl TEXTEQU <usvfsAsmHookManagerDtorImpl>
_usvfsAsmHookManagerDetourImpl TEXTEQU <usvfsAsmHookManagerDetourImpl>
_usvfsAsmHookManagerRemoveHookImpl TEXTEQU <usvfsAsmHookManagerRemoveHookImpl>
_usvfsAsmHookManagerLogStubIntImpl TEXTEQU <usvfsAsmHookManagerLogStubIntImpl>
_usvfsAsmHookManagerLogStubImpl TEXTEQU <usvfsAsmHookManagerLogStubImpl>
_usvfsAsmHookManagerInstallHookImpl TEXTEQU <usvfsAsmHookManagerInstallHookImpl>
_usvfsAsmHookManagerInstallStubImpl TEXTEQU <usvfsAsmHookManagerInstallStubImpl>
_usvfsAsmHookManagerInitHooksImpl TEXTEQU <usvfsAsmHookManagerInitHooksImpl>
_usvfsAsmHookManagerRemoveHooksImpl TEXTEQU <usvfsAsmHookManagerRemoveHooksImpl>

.CODE

PUBLIC ??0HookContext@usvfs@@QEAA@AEBUusvfsParameters@@PEAUHINSTANCE__@@@Z
??0HookContext@usvfs@@QEAA@AEBUusvfsParameters@@PEAUHINSTANCE__@@@Z PROC
    jmp _usvfsAsmHookContextCtorImpl
??0HookContext@usvfs@@QEAA@AEBUusvfsParameters@@PEAUHINSTANCE__@@@Z ENDP

PUBLIC ??1HookContext@usvfs@@QEAA@XZ
??1HookContext@usvfs@@QEAA@XZ PROC
    jmp _usvfsAsmHookContextDtorImpl
??1HookContext@usvfs@@QEAA@XZ ENDP

PUBLIC ?remove@HookContext@usvfs@@SAXPEBD@Z
?remove@HookContext@usvfs@@SAXPEBD@Z PROC
    jmp _usvfsAsmHookContextRemoveImpl
?remove@HookContext@usvfs@@SAXPEBD@Z ENDP

PUBLIC ?registerProcess@HookContext@usvfs@@QEAAXK@Z
?registerProcess@HookContext@usvfs@@QEAAXK@Z PROC
    jmp _usvfsAsmHookContextRegisterProcessImpl
?registerProcess@HookContext@usvfs@@QEAAXK@Z ENDP

PUBLIC ?unregisterCurrentProcess@HookContext@usvfs@@QEAAXXZ
?unregisterCurrentProcess@HookContext@usvfs@@QEAAXXZ PROC
    jmp _usvfsAsmHookContextUnregisterCurrentProcessImpl
?unregisterCurrentProcess@HookContext@usvfs@@QEAAXXZ ENDP

PUBLIC ?blacklistExecutable@HookContext@usvfs@@QEAAXAEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z
?blacklistExecutable@HookContext@usvfs@@QEAAXAEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z PROC
    jmp _usvfsAsmHookContextBlacklistExecutableImpl
?blacklistExecutable@HookContext@usvfs@@QEAAXAEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z ENDP

PUBLIC ?clearExecutableBlacklist@HookContext@usvfs@@QEAAXXZ
?clearExecutableBlacklist@HookContext@usvfs@@QEAAXXZ PROC
    jmp _usvfsAsmHookContextClearExecutableBlacklistImpl
?clearExecutableBlacklist@HookContext@usvfs@@QEAAXXZ ENDP

PUBLIC ?executableBlacklisted@HookContext@usvfs@@QEBAHPEB_W0@Z
?executableBlacklisted@HookContext@usvfs@@QEBAHPEB_W0@Z PROC
    jmp _usvfsAsmHookContextExecutableBlacklistedImpl
?executableBlacklisted@HookContext@usvfs@@QEBAHPEB_W0@Z ENDP

PUBLIC ?forceLoadLibrary@HookContext@usvfs@@QEAAXAEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@0@Z
?forceLoadLibrary@HookContext@usvfs@@QEAAXAEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@0@Z PROC
    jmp _usvfsAsmHookContextForceLoadLibraryImpl
?forceLoadLibrary@HookContext@usvfs@@QEAAXAEBV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@0@Z ENDP

PUBLIC ?clearLibraryForceLoads@HookContext@usvfs@@QEAAXXZ
?clearLibraryForceLoads@HookContext@usvfs@@QEAAXXZ PROC
    jmp _usvfsAsmHookContextClearLibraryForceLoadsImpl
?clearLibraryForceLoads@HookContext@usvfs@@QEAAXXZ ENDP

PUBLIC ?registerDelayed@HookContext@usvfs@@QEAAXV?$future@H@std@@@Z
?registerDelayed@HookContext@usvfs@@QEAAXV?$future@H@std@@@Z PROC
    jmp _usvfsAsmHookContextRegisterDelayedImpl
?registerDelayed@HookContext@usvfs@@QEAAXV?$future@H@std@@@Z ENDP

PUBLIC ?unlock@HookContext@usvfs@@CAXPEAV12@@Z
?unlock@HookContext@usvfs@@CAXPEAV12@@Z PROC
    jmp _usvfsAsmHookContextUnlockImpl
?unlock@HookContext@usvfs@@CAXPEAV12@@Z ENDP

PUBLIC ?unlockShared@HookContext@usvfs@@CAXPEBV12@@Z
?unlockShared@HookContext@usvfs@@CAXPEBV12@@Z PROC
    jmp _usvfsAsmHookContextUnlockSharedImpl
?unlockShared@HookContext@usvfs@@CAXPEBV12@@Z ENDP

PUBLIC ?retrieveParameters@HookContext@usvfs@@AEAAPEAVSharedParameters@2@AEBUusvfsParameters@@@Z
?retrieveParameters@HookContext@usvfs@@AEAAPEAVSharedParameters@2@AEBUusvfsParameters@@@Z PROC
    jmp _usvfsAsmHookContextRetrieveParametersImpl
?retrieveParameters@HookContext@usvfs@@AEAAPEAVSharedParameters@2@AEBUusvfsParameters@@@Z ENDP

PUBLIC ??0HookManager@usvfs@@QEAA@AEBUusvfsParameters@@PEAUHINSTANCE__@@@Z
??0HookManager@usvfs@@QEAA@AEBUusvfsParameters@@PEAUHINSTANCE__@@@Z PROC
    jmp _usvfsAsmHookManagerCtorImpl
??0HookManager@usvfs@@QEAA@AEBUusvfsParameters@@PEAUHINSTANCE__@@@Z ENDP

PUBLIC ??1HookManager@usvfs@@QEAA@XZ
??1HookManager@usvfs@@QEAA@XZ PROC
    jmp _usvfsAsmHookManagerDtorImpl
??1HookManager@usvfs@@QEAA@XZ ENDP

PUBLIC ?detour@HookManager@usvfs@@QEAAPEAXPEBD@Z
?detour@HookManager@usvfs@@QEAAPEAXPEBD@Z PROC
    jmp _usvfsAsmHookManagerDetourImpl
?detour@HookManager@usvfs@@QEAAPEAXPEBD@Z ENDP

PUBLIC ?removeHook@HookManager@usvfs@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
?removeHook@HookManager@usvfs@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC
    jmp _usvfsAsmHookManagerRemoveHookImpl
?removeHook@HookManager@usvfs@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

PUBLIC ?logStubInt@HookManager@usvfs@@AEAAXPEAX@Z
?logStubInt@HookManager@usvfs@@AEAAXPEAX@Z PROC
    jmp _usvfsAsmHookManagerLogStubIntImpl
?logStubInt@HookManager@usvfs@@AEAAXPEAX@Z ENDP

PUBLIC ?logStub@HookManager@usvfs@@CAXPEAX@Z
?logStub@HookManager@usvfs@@CAXPEAX@Z PROC
    jmp _usvfsAsmHookManagerLogStubImpl
?logStub@HookManager@usvfs@@CAXPEAX@Z ENDP

PUBLIC ?installHook@HookManager@usvfs@@AEAAXPEAUHINSTANCE__@@0AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEAXPEAPEAX@Z
?installHook@HookManager@usvfs@@AEAAXPEAUHINSTANCE__@@0AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEAXPEAPEAX@Z PROC
    jmp _usvfsAsmHookManagerInstallHookImpl
?installHook@HookManager@usvfs@@AEAAXPEAUHINSTANCE__@@0AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEAXPEAPEAX@Z ENDP

PUBLIC ?installStub@HookManager@usvfs@@AEAAXPEAUHINSTANCE__@@0AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
?installStub@HookManager@usvfs@@AEAAXPEAUHINSTANCE__@@0AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC
    jmp _usvfsAsmHookManagerInstallStubImpl
?installStub@HookManager@usvfs@@AEAAXPEAUHINSTANCE__@@0AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

PUBLIC ?initHooks@HookManager@usvfs@@AEAAXXZ
?initHooks@HookManager@usvfs@@AEAAXXZ PROC
    jmp _usvfsAsmHookManagerInitHooksImpl
?initHooks@HookManager@usvfs@@AEAAXXZ ENDP

PUBLIC ?removeHooks@HookManager@usvfs@@AEAAXXZ
?removeHooks@HookManager@usvfs@@AEAAXXZ PROC
    jmp _usvfsAsmHookManagerRemoveHooksImpl
?removeHooks@HookManager@usvfs@@AEAAXXZ ENDP

END
