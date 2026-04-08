OPTION CASEMAP:NONE

EXTERN __imp_CloseHandle:QWORD
EXTERN __imp_CreateSemaphoreW:QWORD
EXTERN __imp_GetCurrentThreadId:QWORD
EXTERN __imp_GetLastError:QWORD
EXTERN __imp_GetProcessHeap:QWORD
EXTERN __imp_HeapAlloc:QWORD
EXTERN __imp_OpenThread:QWORD
EXTERN __imp_ReleaseSemaphore:QWORD
EXTERN __imp_SetLastError:QWORD
EXTERN __imp_TlsAlloc:QWORD
EXTERN __imp_TlsGetValue:QWORD
EXTERN __imp_TlsSetValue:QWORD
EXTERN __imp_WaitForSingleObject:QWORD

EXTERN _usvfsAsmRecursiveBenaphoreOwnerDiedLogImpl:PROC
EXTERN _usvfsAsmStreamRedirectionDataImpl:PROC

.DATA
ALIGN 4
_usvfsAsmHookStackTlsState DWORD 0
_usvfsAsmHookStackTlsIndex DWORD 0FFFFFFFFh
_usvfsAsmHookStackFallbackMask DWORD 0

.CODE

_usvfsAsmHookStackOutOfRange PROC
    int 3
    ret
_usvfsAsmHookStackOutOfRange ENDP

_usvfsAsmHookStackSetGroup PROC
    cmp edx, 0Ch
    jae _usvfsAsmHookStackSetGroupOutOfRange

    bt dword ptr [rcx], edx
    jc _usvfsAsmHookStackSetGroupBusy

    bt dword ptr [rcx], 0
    jc _usvfsAsmHookStackSetGroupBusy

    mov eax, 1
    mov r8, rcx
    mov ecx, edx
    shl eax, cl
    or dword ptr [r8], eax
    mov al, 1
    ret

_usvfsAsmHookStackSetGroupBusy:
    xor eax, eax
    ret

_usvfsAsmHookStackSetGroupOutOfRange:
    jmp _usvfsAsmHookStackOutOfRange
_usvfsAsmHookStackSetGroup ENDP

_usvfsAsmHookStackClearGroup PROC
    cmp edx, 0Ch
    jae _usvfsAsmHookStackClearGroupOutOfRange

    btr dword ptr [rcx], edx
    ret

_usvfsAsmHookStackClearGroupOutOfRange:
    jmp _usvfsAsmHookStackOutOfRange
_usvfsAsmHookStackClearGroup ENDP

PUBLIC ?instance@HookStack@usvfs@@SAAEAV12@XZ
?instance@HookStack@usvfs@@SAAEAV12@XZ PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog

    call qword ptr [__imp_GetLastError]
    mov dword ptr [rsp+20h], eax

_usvfsAsmHookStackCheckInit:
    mov eax, dword ptr [_usvfsAsmHookStackTlsState]
    cmp eax, 2
    je _usvfsAsmHookStackReady

    cmp eax, 0
    jne _usvfsAsmHookStackSpin

    xor eax, eax
    mov ecx, 1
    lock cmpxchg dword ptr [_usvfsAsmHookStackTlsState], ecx
    jne _usvfsAsmHookStackCheckInit

    call qword ptr [__imp_TlsAlloc]
    mov dword ptr [_usvfsAsmHookStackTlsIndex], eax
    mov dword ptr [_usvfsAsmHookStackTlsState], 2
    jmp _usvfsAsmHookStackReady

_usvfsAsmHookStackSpin:
    pause
    jmp _usvfsAsmHookStackCheckInit

_usvfsAsmHookStackReady:
    mov ecx, dword ptr [_usvfsAsmHookStackTlsIndex]
    cmp ecx, 0FFFFFFFFh
    je _usvfsAsmHookStackFallback

    call qword ptr [__imp_TlsGetValue]
    test rax, rax
    jne _usvfsAsmHookStackRestore

    call qword ptr [__imp_GetProcessHeap]
    mov rcx, rax
    mov edx, 8
    mov r8d, 4
    call qword ptr [__imp_HeapAlloc]
    test rax, rax
    je _usvfsAsmHookStackFallback

    mov qword ptr [rsp+28h], rax
    mov ecx, dword ptr [_usvfsAsmHookStackTlsIndex]
    mov rdx, rax
    call qword ptr [__imp_TlsSetValue]
    mov rax, qword ptr [rsp+28h]
    jmp _usvfsAsmHookStackRestore

_usvfsAsmHookStackFallback:
    lea rax, _usvfsAsmHookStackFallbackMask

_usvfsAsmHookStackRestore:
    mov ecx, dword ptr [rsp+20h]
    call qword ptr [__imp_SetLastError]

    add rsp, 38h
    ret
?instance@HookStack@usvfs@@SAAEAV12@XZ ENDP

PUBLIC ??0HookCallContext@usvfs@@QEAA@XZ
??0HookCallContext@usvfs@@QEAA@XZ PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov qword ptr [rsp+20h], rcx
    mov byte ptr [rcx+4], 1
    mov dword ptr [rcx+8], 0Ch

    call qword ptr [__imp_GetLastError]

    mov rcx, qword ptr [rsp+20h]
    mov dword ptr [rcx], eax
    mov rax, rcx

    add rsp, 28h
    ret
??0HookCallContext@usvfs@@QEAA@XZ ENDP

PUBLIC ??0HookCallContext@usvfs@@QEAA@W4MutExHookGroup@1@@Z
??0HookCallContext@usvfs@@QEAA@W4MutExHookGroup@1@@Z PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog

    mov qword ptr [rsp+20h], rcx
    mov dword ptr [rsp+28h], edx

    call ?instance@HookStack@usvfs@@SAAEAV12@XZ
    mov rcx, rax
    mov edx, dword ptr [rsp+28h]
    call _usvfsAsmHookStackSetGroup

    mov rcx, qword ptr [rsp+20h]
    mov byte ptr [rcx+4], al
    mov edx, dword ptr [rsp+28h]
    mov dword ptr [rcx+8], edx

    call qword ptr [__imp_GetLastError]

    mov rcx, qword ptr [rsp+20h]
    mov dword ptr [rcx], eax
    mov rax, rcx

    add rsp, 38h
    ret
??0HookCallContext@usvfs@@QEAA@W4MutExHookGroup@1@@Z ENDP

PUBLIC ??1HookCallContext@usvfs@@QEAA@XZ
??1HookCallContext@usvfs@@QEAA@XZ PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov qword ptr [rsp+20h], rcx

    cmp byte ptr [rcx+4], 0
    je _usvfsAsmHookCallContextSetLastError

    cmp dword ptr [rcx+8], 0Ch
    je _usvfsAsmHookCallContextSetLastError

    call ?instance@HookStack@usvfs@@SAAEAV12@XZ
    mov rcx, rax
    mov rdx, qword ptr [rsp+20h]
    mov edx, dword ptr [rdx+8]
    call _usvfsAsmHookStackClearGroup

_usvfsAsmHookCallContextSetLastError:
    mov rcx, qword ptr [rsp+20h]
    mov ecx, dword ptr [rcx]
    call qword ptr [__imp_SetLastError]

    add rsp, 28h
    ret
??1HookCallContext@usvfs@@QEAA@XZ ENDP

PUBLIC ?restoreLastError@HookCallContext@usvfs@@QEAAXXZ
?restoreLastError@HookCallContext@usvfs@@QEAAXXZ PROC
    mov ecx, dword ptr [rcx]
    jmp qword ptr [__imp_SetLastError]
?restoreLastError@HookCallContext@usvfs@@QEAAXXZ ENDP

PUBLIC ?updateLastError@HookCallContext@usvfs@@QEAAXK@Z
?updateLastError@HookCallContext@usvfs@@QEAAXK@Z PROC
    mov dword ptr [rcx], edx
    ret
?updateLastError@HookCallContext@usvfs@@QEAAXK@Z ENDP

PUBLIC ?active@HookCallContext@usvfs@@QEBA_NXZ
?active@HookCallContext@usvfs@@QEBA_NXZ PROC
    movzx eax, byte ptr [rcx+4]
    ret
?active@HookCallContext@usvfs@@QEBA_NXZ ENDP

PUBLIC ??0FunctionGroupLock@usvfs@@QEAA@W4MutExHookGroup@1@@Z
??0FunctionGroupLock@usvfs@@QEAA@W4MutExHookGroup@1@@Z PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog

    mov qword ptr [rsp+20h], rcx
    mov dword ptr [rcx], edx

    call ?instance@HookStack@usvfs@@SAAEAV12@XZ
    mov rcx, rax
    mov rdx, qword ptr [rsp+20h]
    mov edx, dword ptr [rdx]
    call _usvfsAsmHookStackSetGroup

    mov rcx, qword ptr [rsp+20h]
    mov byte ptr [rcx+4], al
    mov rax, rcx

    add rsp, 38h
    ret
??0FunctionGroupLock@usvfs@@QEAA@W4MutExHookGroup@1@@Z ENDP

PUBLIC ??1FunctionGroupLock@usvfs@@QEAA@XZ
??1FunctionGroupLock@usvfs@@QEAA@XZ PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov qword ptr [rsp+20h], rcx
    cmp byte ptr [rcx+4], 0
    je _usvfsAsmFunctionGroupLockDone

    call ?instance@HookStack@usvfs@@SAAEAV12@XZ
    mov rcx, rax
    mov rdx, qword ptr [rsp+20h]
    mov edx, dword ptr [rdx]
    call _usvfsAsmHookStackClearGroup

_usvfsAsmFunctionGroupLockDone:
    add rsp, 28h
    ret
??1FunctionGroupLock@usvfs@@QEAA@XZ ENDP

PUBLIC ??6usvfs@@YAAEAV?$basic_ostream@DU?$char_traits@D@std@@@std@@AEAV12@AEBURedirectionData@0@@Z
??6usvfs@@YAAEAV?$basic_ostream@DU?$char_traits@D@std@@@std@@AEAV12@AEBURedirectionData@0@@Z PROC
    jmp _usvfsAsmStreamRedirectionDataImpl
??6usvfs@@YAAEAV?$basic_ostream@DU?$char_traits@D@std@@@std@@AEAV12@AEBURedirectionData@0@@Z ENDP

PUBLIC ??0RecursiveBenaphore@@QEAA@XZ
??0RecursiveBenaphore@@QEAA@XZ PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov qword ptr [rsp+20h], rcx
    xor eax, eax
    mov qword ptr [rcx], rax
    mov dword ptr [rcx+8], eax

    xor ecx, ecx
    mov edx, 1
    mov r8d, 1
    xor r9d, r9d
    call qword ptr [__imp_CreateSemaphoreW]

    mov rcx, qword ptr [rsp+20h]
    mov qword ptr [rcx+10h], rax
    mov rax, rcx

    add rsp, 28h
    ret
??0RecursiveBenaphore@@QEAA@XZ ENDP

PUBLIC ??1RecursiveBenaphore@@QEAA@XZ
??1RecursiveBenaphore@@QEAA@XZ PROC
    mov rcx, qword ptr [rcx+10h]
    jmp qword ptr [__imp_CloseHandle]
??1RecursiveBenaphore@@QEAA@XZ ENDP

PUBLIC ?wait@RecursiveBenaphore@@QEAAXK@Z
?wait@RecursiveBenaphore@@QEAAXK@Z PROC FRAME
    sub rsp, 58h
    .allocstack 58h
    .endprolog

    mov qword ptr [rsp+20h], rcx
    mov dword ptr [rsp+28h], edx

    call qword ptr [__imp_GetCurrentThreadId]
    mov dword ptr [rsp+2Ch], eax

    mov rcx, qword ptr [rsp+20h]
    mov eax, 1
    lock xadd dword ptr [rcx], eax
    inc eax
    cmp eax, 1
    jle _usvfsAsmRecursiveBenaphoreWaitDone

    mov edx, dword ptr [rsp+2Ch]
    cmp edx, dword ptr [rcx+4]
    je _usvfsAsmRecursiveBenaphoreWaitDone

    mov dword ptr [rsp+30h], 3

_usvfsAsmRecursiveBenaphoreWaitLoop:
    mov rcx, qword ptr [rsp+20h]
    mov rcx, qword ptr [rcx+10h]
    mov edx, dword ptr [rsp+28h]
    call qword ptr [__imp_WaitForSingleObject]
    test eax, eax
    je _usvfsAsmRecursiveBenaphoreWaitDone

    mov rcx, qword ptr [rsp+20h]
    mov r8d, dword ptr [rcx+4]
    mov ecx, 100000h
    xor edx, edx
    call qword ptr [__imp_OpenThread]
    mov qword ptr [rsp+38h], rax

    cmp dword ptr [rsp+30h], 0
    jle _usvfsAsmRecursiveBenaphoreSteal

    xor edx, edx
    mov rcx, rax
    call qword ptr [__imp_WaitForSingleObject]
    test eax, eax
    je _usvfsAsmRecursiveBenaphoreSteal

    sub dword ptr [rsp+30h], 1
    mov rcx, qword ptr [rsp+38h]
    call qword ptr [__imp_CloseHandle]
    jmp _usvfsAsmRecursiveBenaphoreWaitLoop

_usvfsAsmRecursiveBenaphoreSteal:
    mov rcx, qword ptr [rsp+20h]
    mov dword ptr [rcx+8], 0
    mov ecx, dword ptr [rcx+4]
    call _usvfsAsmRecursiveBenaphoreOwnerDiedLogImpl
    mov rcx, qword ptr [rsp+38h]
    call qword ptr [__imp_CloseHandle]

_usvfsAsmRecursiveBenaphoreWaitDone:
    mov rcx, qword ptr [rsp+20h]
    mov edx, dword ptr [rsp+2Ch]
    mov dword ptr [rcx+4], edx
    inc dword ptr [rcx+8]

    add rsp, 58h
    ret
?wait@RecursiveBenaphore@@QEAAXK@Z ENDP

PUBLIC ?signal@RecursiveBenaphore@@QEAAXXZ
?signal@RecursiveBenaphore@@QEAAXXZ PROC
    mov edx, dword ptr [rcx+8]
    test edx, edx
    je _usvfsAsmRecursiveBenaphoreSignalDone

    sub edx, 1
    mov dword ptr [rcx+8], edx
    jne _usvfsAsmRecursiveBenaphoreSignalDecrement

    mov dword ptr [rcx+4], 0

_usvfsAsmRecursiveBenaphoreSignalDecrement:
    mov eax, 0FFFFFFFFh
    lock xadd dword ptr [rcx], eax
    cmp eax, 1
    je _usvfsAsmRecursiveBenaphoreSignalDone

    test edx, edx
    jne _usvfsAsmRecursiveBenaphoreSignalDone

    mov rcx, qword ptr [rcx+10h]
    mov edx, 1
    xor r8d, r8d
    jmp qword ptr [__imp_ReleaseSemaphore]

_usvfsAsmRecursiveBenaphoreSignalDone:
    ret
?signal@RecursiveBenaphore@@QEAAXXZ ENDP

END
