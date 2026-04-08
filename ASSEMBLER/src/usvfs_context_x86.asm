.686
.MODEL FLAT
OPTION CASEMAP:NONE

; External Impl functions (cdecl in bridge)
; NOTE: These are __cdecl, so the CALLER (the asm wrapper) must clean up the stack.
EXTERN _usvfsAsmHookContextCtorImpl:PROC
EXTERN _usvfsAsmHookContextDtorImpl:PROC
EXTERN _usvfsAsmHookContextRemoveImpl:PROC
EXTERN _usvfsAsmHookContextRegisterProcessImpl:PROC
EXTERN _usvfsAsmHookContextUnregisterCurrentProcessImpl:PROC
EXTERN _usvfsAsmHookContextBlacklistExecutableImpl:PROC
EXTERN _usvfsAsmHookContextClearExecutableBlacklistImpl:PROC
EXTERN _usvfsAsmHookContextExecutableBlacklistedImpl:PROC
EXTERN _usvfsAsmHookContextForceLoadLibraryImpl:PROC
EXTERN _usvfsAsmHookContextClearLibraryForceLoadsImpl:PROC
EXTERN _usvfsAsmHookContextRegisterDelayedImpl:PROC
EXTERN _usvfsAsmHookContextUnlockImpl:PROC
EXTERN _usvfsAsmHookContextUnlockSharedImpl:PROC
EXTERN _usvfsAsmHookContextRetrieveParametersImpl:PROC

EXTERN _usvfsAsmHookManagerCtorImpl:PROC
EXTERN _usvfsAsmHookManagerDtorImpl:PROC
EXTERN _usvfsAsmHookManagerDetourImpl:PROC
EXTERN _usvfsAsmHookManagerRemoveHookImpl:PROC
EXTERN _usvfsAsmHookManagerLogStubIntImpl:PROC
EXTERN _usvfsAsmHookManagerLogStubImpl:PROC
EXTERN _usvfsAsmHookManagerInstallHookImpl:PROC
EXTERN _usvfsAsmHookManagerInstallStubImpl:PROC
EXTERN _usvfsAsmHookManagerInitHooksImpl:PROC
EXTERN _usvfsAsmHookManagerRemoveHooksImpl:PROC

.CODE

; ---------------------------------------------------------------
; HookContext member trampolines (thiscall)
; ecx = this, stack has args. Callee (us) cleans up original args.
; ---------------------------------------------------------------

PUBLIC ??0HookContext@usvfs@@QAE@ABUusvfsParameters@@PAUHINSTANCE__@@@Z
??0HookContext@usvfs@@QAE@ABUusvfsParameters@@PAUHINSTANCE__@@@Z PROC
    ; stack: ret, p1, p2
    push [esp+8] ; hinst
    push [esp+8] ; params
    push ecx     ; this
    call _usvfsAsmHookContextCtorImpl
    add esp, 12
    ret 8
??0HookContext@usvfs@@QAE@ABUusvfsParameters@@PAUHINSTANCE__@@@Z ENDP

PUBLIC ??1HookContext@usvfs@@QAE@XZ
??1HookContext@usvfs@@QAE@XZ PROC
    push ecx
    call _usvfsAsmHookContextDtorImpl
    add esp, 4
    ret
??1HookContext@usvfs@@QAE@XZ ENDP

; Static method (cdecl usually, but mangled as _SAX)
PUBLIC ?remove@HookContext@usvfs@@SAXPBD@Z
?remove@HookContext@usvfs@@SAXPBD@Z PROC
    jmp _usvfsAsmHookContextRemoveImpl
?remove@HookContext@usvfs@@SAXPBD@Z ENDP

PUBLIC ?registerProcess@HookContext@usvfs@@QAEXK@Z
?registerProcess@HookContext@usvfs@@QAEXK@Z PROC
    push [esp+4] ; pid
    push ecx     ; this
    call _usvfsAsmHookContextRegisterProcessImpl
    add esp, 8
    ret 4
?registerProcess@HookContext@usvfs@@QAEXK@Z ENDP

PUBLIC ?unregisterCurrentProcess@HookContext@usvfs@@QAEXXZ
?unregisterCurrentProcess@HookContext@usvfs@@QAEXXZ PROC
    push ecx
    call _usvfsAsmHookContextUnregisterCurrentProcessImpl
    add esp, 4
    ret
?unregisterCurrentProcess@HookContext@usvfs@@QAEXXZ ENDP

PUBLIC ?blacklistExecutable@HookContext@usvfs@@QAEXABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z
?blacklistExecutable@HookContext@usvfs@@QAEXABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z PROC
    push [esp+4]
    push ecx
    call _usvfsAsmHookContextBlacklistExecutableImpl
    add esp, 8
    ret 4
?blacklistExecutable@HookContext@usvfs@@QAEXABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@@Z ENDP

PUBLIC ?clearExecutableBlacklist@HookContext@usvfs@@QAEXXZ
?clearExecutableBlacklist@HookContext@usvfs@@QAEXXZ PROC
    push ecx
    call _usvfsAsmHookContextClearExecutableBlacklistImpl
    add esp, 4
    ret
?clearExecutableBlacklist@HookContext@usvfs@@QAEXXZ ENDP

PUBLIC ?executableBlacklisted@HookContext@usvfs@@QBEHPB_W0@Z
?executableBlacklisted@HookContext@usvfs@@QBEHPB_W0@Z PROC
    push [esp+8]
    push [esp+8]
    push ecx
    call _usvfsAsmHookContextExecutableBlacklistedImpl
    add esp, 12
    ret 8
?executableBlacklisted@HookContext@usvfs@@QBEHPB_W0@Z ENDP

PUBLIC ?forceLoadLibrary@HookContext@usvfs@@QAEXABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@0@Z
?forceLoadLibrary@HookContext@usvfs@@QAEXABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@0@Z PROC
    push [esp+8]
    push [esp+8]
    push ecx
    call _usvfsAsmHookContextForceLoadLibraryImpl
    add esp, 12
    ret 8
?forceLoadLibrary@HookContext@usvfs@@QAEXABV?$basic_string@_WU?$char_traits@_W@std@@V?$allocator@_W@2@@std@@0@Z ENDP

PUBLIC ?clearLibraryForceLoads@HookContext@usvfs@@QAEXXZ
?clearLibraryForceLoads@HookContext@usvfs@@QAEXXZ PROC
    push ecx
    call _usvfsAsmHookContextClearLibraryForceLoadsImpl
    add esp, 4
    ret
?clearLibraryForceLoads@HookContext@usvfs@@QAEXXZ ENDP

; RegisterDelayed has a std::future argument, which on x86 might be a pointer or small struct.
; Usually it's passed by value (struct).
PUBLIC ?registerDelayed@HookContext@usvfs@@QAEXV?$future@H@std@@@Z
?registerDelayed@HookContext@usvfs@@QAEXV?$future@H@std@@@Z PROC
    push [esp+4]
    push ecx
    call _usvfsAsmHookContextRegisterDelayedImpl
    add esp, 8
    ret 4
?registerDelayed@HookContext@usvfs@@QAEXV?$future@H@std@@@Z ENDP

; Static methods
PUBLIC ?unlock@HookContext@usvfs@@CAXPAV12@@Z
?unlock@HookContext@usvfs@@CAXPAV12@@Z PROC
    jmp _usvfsAsmHookContextUnlockImpl
?unlock@HookContext@usvfs@@CAXPAV12@@Z ENDP

PUBLIC ?unlockShared@HookContext@usvfs@@CAXPBV12@@Z
?unlockShared@HookContext@usvfs@@CAXPBV12@@Z PROC
    jmp _usvfsAsmHookContextUnlockSharedImpl
?unlockShared@HookContext@usvfs@@CAXPBV12@@Z ENDP

PUBLIC ?retrieveParameters@HookContext@usvfs@@AAEPAVSharedParameters@2@ABUusvfsParameters@@@Z
?retrieveParameters@HookContext@usvfs@@AAEPAVSharedParameters@2@ABUusvfsParameters@@@Z PROC
    push [esp+4]
    push ecx
    call _usvfsAsmHookContextRetrieveParametersImpl
    add esp, 8
    ret 4
?retrieveParameters@HookContext@usvfs@@AAEPAVSharedParameters@2@ABUusvfsParameters@@@Z ENDP

; ---------------------------------------------------------------
; HookManager member trampolines (thiscall)
; ---------------------------------------------------------------

PUBLIC ??0HookManager@usvfs@@QAE@ABUusvfsParameters@@PAUHINSTANCE__@@@Z
??0HookManager@usvfs@@QAE@ABUusvfsParameters@@PAUHINSTANCE__@@@Z PROC
    push [esp+8]
    push [esp+8]
    push ecx
    call _usvfsAsmHookManagerCtorImpl
    add esp, 12
    ret 8
??0HookManager@usvfs@@QAE@ABUusvfsParameters@@PAUHINSTANCE__@@@Z ENDP

PUBLIC ??1HookManager@usvfs@@QAE@XZ
??1HookManager@usvfs@@QAE@XZ PROC
    push ecx
    call _usvfsAsmHookManagerDtorImpl
    add esp, 4
    ret
??1HookManager@usvfs@@QAE@XZ ENDP

PUBLIC ?detour@HookManager@usvfs@@QAEPBXPBD@Z
?detour@HookManager@usvfs@@QAEPBXPBD@Z PROC
    push [esp+4]
    push ecx
    call _usvfsAsmHookManagerDetourImpl
    add esp, 8
    ret 4
?detour@HookManager@usvfs@@QAEPBXPBD@Z ENDP

PUBLIC ?removeHook@HookManager@usvfs@@QAEXABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
?removeHook@HookManager@usvfs@@QAEXABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC
    push [esp+4]
    push ecx
    call _usvfsAsmHookManagerRemoveHookImpl
    add esp, 8
    ret 4
?removeHook@HookManager@usvfs@@QAEXABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

PUBLIC ?logStubInt@HookManager@usvfs@@AAEXPAX@Z
?logStubInt@HookManager@usvfs@@AAEXPAX@Z PROC
    push [esp+4]
    push ecx
    call _usvfsAsmHookManagerLogStubIntImpl
    add esp, 8
    ret 4
?logStubInt@HookManager@usvfs@@AAEXPAX@Z ENDP

; Static method
PUBLIC ?logStub@HookManager@usvfs@@CAXPAX@Z
?logStub@HookManager@usvfs@@CAXPAX@Z PROC
    jmp _usvfsAsmHookManagerLogStubImpl
?logStub@HookManager@usvfs@@CAXPAX@Z ENDP

PUBLIC ?installHook@HookManager@usvfs@@AAEXPAUHINSTANCE__@@0ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PAXPAPAX@Z
?installHook@HookManager@usvfs@@AAEXPAUHINSTANCE__@@0ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PAXPAPAX@Z PROC
    push [esp+20]
    push [esp+20]
    push [esp+20]
    push [esp+20]
    push [esp+20]
    push ecx
    call _usvfsAsmHookManagerInstallHookImpl
    add esp, 24
    ret 20
?installHook@HookManager@usvfs@@AAEXPAUHINSTANCE__@@0ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PAXPAPAX@Z ENDP

PUBLIC ?installStub@HookManager@usvfs@@AAEXPAUHINSTANCE__@@0ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
?installStub@HookManager@usvfs@@AAEXPAUHINSTANCE__@@0ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC
    push [esp+12]
    push [esp+12]
    push [esp+12]
    push ecx
    call _usvfsAsmHookManagerInstallStubImpl
    add esp, 16
    ret 12
?installStub@HookManager@usvfs@@AAEXPAUHINSTANCE__@@0ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

PUBLIC ?initHooks@HookManager@usvfs@@AAEXXZ
?initHooks@HookManager@usvfs@@AAEXXZ PROC
    push ecx
    call _usvfsAsmHookManagerInitHooksImpl
    add esp, 4
    ret
?initHooks@HookManager@usvfs@@AAEXXZ ENDP

PUBLIC ?removeHooks@HookManager@usvfs@@AAEXXZ
?removeHooks@HookManager@usvfs@@AAEXXZ PROC
    push ecx
    call _usvfsAsmHookManagerRemoveHooksImpl
    add esp, 4
    ret
?removeHooks@HookManager@usvfs@@AAEXXZ ENDP

END
