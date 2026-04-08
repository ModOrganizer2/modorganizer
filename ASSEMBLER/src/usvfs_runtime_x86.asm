.686
.MODEL FLAT
OPTION CASEMAP:NONE

; External Windows APIs (stdcall)
EXTERN __imp__CloseHandle@4:DWORD
EXTERN __imp__CreateSemaphoreW@16:DWORD
EXTERN __imp__GetCurrentThreadId@0:DWORD
EXTERN __imp__GetLastError@0:DWORD
EXTERN __imp__GetProcessHeap@0:DWORD
EXTERN __imp__HeapAlloc@12:DWORD
EXTERN __imp__OpenThread@12:DWORD
EXTERN __imp__ReleaseSemaphore@12:DWORD
EXTERN __imp__SetLastError@4:DWORD
EXTERN __imp__TlsAlloc@0:DWORD
EXTERN __imp__TlsGetValue@4:DWORD
EXTERN __imp__TlsSetValue@8:DWORD
EXTERN __imp__WaitForSingleObject@8:DWORD

; External Bridge functions (cdecl)
EXTERN _usvfsAsmRecursiveBenaphoreOwnerDiedLogImpl:PROC
EXTERN _usvfsAsmStreamRedirectionDataImpl:PROC

.DATA
ALIGN 4
_usvfsAsmHookStackTlsState DWORD 0
_usvfsAsmHookStackTlsIndex DWORD 0FFFFFFFFh
_usvfsAsmHookStackFallbackMask DWORD 0

.CODE

; ---------------------------------------------------------------
; _usvfsAsmHookStackOutOfRange — int 3
; ---------------------------------------------------------------
_usvfsAsmHookStackOutOfRange PROC
    int 3
    ret
_usvfsAsmHookStackOutOfRange ENDP

; ---------------------------------------------------------------
; _usvfsAsmHookStackSetGroup(ecx = mask, edx = group)
; ---------------------------------------------------------------
_usvfsAsmHookStackSetGroup PROC
    cmp edx, 0Ch
    jae _usvfsAsmHookStackSetGroupOutOfRange
    
    bt dword ptr [ecx], edx
    jc _usvfsAsmHookStackSetGroupBusy
    
    bt dword ptr [ecx], 0
    jc _usvfsAsmHookStackSetGroupBusy
    
    push ebx
    mov ebx, 1
    push ecx
    mov ecx, edx
    shl ebx, cl
    pop ecx
    or dword ptr [ecx], ebx
    pop ebx
    mov al, 1
    ret
    
_usvfsAsmHookStackSetGroupBusy:
    xor eax, eax
    ret
    
_usvfsAsmHookStackSetGroupOutOfRange:
    jmp _usvfsAsmHookStackOutOfRange
_usvfsAsmHookStackSetGroup ENDP

; ---------------------------------------------------------------
; _usvfsAsmHookStackClearGroup(ecx = mask, edx = group)
; ---------------------------------------------------------------
_usvfsAsmHookStackClearGroup PROC
    cmp edx, 0Ch
    jae _usvfsAsmHookStackClearGroupOutOfRange
    btr dword ptr [ecx], edx
    ret
_usvfsAsmHookStackClearGroupOutOfRange:
    jmp _usvfsAsmHookStackOutOfRange
_usvfsAsmHookStackClearGroup ENDP

; ---------------------------------------------------------------
; HookStack::instance() (stdcall-ish, static)
; Returns EAX = instance pointer
; ---------------------------------------------------------------
PUBLIC ?instance@HookStack@usvfs@@SAAEAV12@XZ
?instance@HookStack@usvfs@@SAAEAV12@XZ PROC
    push ebp
    mov ebp, esp
    push ebx
    push edi
    push esi
    
    call dword ptr [__imp__GetLastError@0]
    push eax ; save last error
    
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
    
    call dword ptr [__imp__TlsAlloc@0]
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
    
    ; Fast TLS access: mov eax, fs:[2Ch] / mov eax, [eax + index*4]
    ; But let's stick to API for now to be safe, or use the fast path.
    push ecx
    call dword ptr [__imp__TlsGetValue@4]
    test eax, eax
    jne _usvfsAsmHookStackRestore
    
    ; Allocate 4 bytes for the mask
    push 4 ; size
    push 8 ; HEAP_ZERO_MEMORY
    call dword ptr [__imp__GetProcessHeap@0]
    push eax
    call dword ptr [__imp__HeapAlloc@12]
    test eax, eax
    je _usvfsAsmHookStackFallback
    
    push eax ; save mask ptr
    push eax ; lpTlsValue
    push dword ptr [_usvfsAsmHookStackTlsIndex] ; dwTlsIndex
    call dword ptr [__imp__TlsSetValue@8]
    pop eax ; restore mask ptr
    jmp _usvfsAsmHookStackRestore
    
_usvfsAsmHookStackFallback:
    mov eax, offset _usvfsAsmHookStackFallbackMask
    
_usvfsAsmHookStackRestore:
    pop ecx ; restore last error
    push eax ; save instance
    push ecx
    call dword ptr [__imp__SetLastError@4]
    pop eax ; restore instance
    
    pop esi
    pop edi
    pop ebx
    pop ebp
    ret
?instance@HookStack@usvfs@@SAAEAV12@XZ ENDP

; ---------------------------------------------------------------
; HookCallContext default constructor (thiscall)
; ---------------------------------------------------------------
PUBLIC ??0HookCallContext@usvfs@@QAE@XZ
??0HookCallContext@usvfs@@QAE@XZ PROC
    push edi
    mov edi, ecx
    mov byte ptr [edi+4], 1
    mov dword ptr [edi+8], 0Ch
    
    call dword ptr [__imp__GetLastError@0]
    mov dword ptr [edi], eax
    mov eax, edi
    pop edi
    ret
??0HookCallContext@usvfs@@QAE@XZ ENDP

; ---------------------------------------------------------------
; HookCallContext parameterized constructor (thiscall)
; ---------------------------------------------------------------
PUBLIC ??0HookCallContext@usvfs@@QAE@W4MutExHookGroup@1@@Z
??0HookCallContext@usvfs@@QAE@W4MutExHookGroup@1@@Z PROC
    push ebp
    mov ebp, esp
    push edi
    push esi
    
    mov edi, ecx ; this
    mov esi, dword ptr [ebp+8] ; group
    
    call ?instance@HookStack@usvfs@@SAAEAV12@XZ
    mov ecx, eax
    mov edx, esi
    call _usvfsAsmHookStackSetGroup
    
    mov byte ptr [edi+4], al
    mov dword ptr [edi+8], esi
    
    call dword ptr [__imp__GetLastError@0]
    mov dword ptr [edi], eax
    mov eax, edi
    
    pop esi
    pop edi
    pop ebp
    ret 4
??0HookCallContext@usvfs@@QAE@W4MutExHookGroup@1@@Z ENDP

; ---------------------------------------------------------------
; HookCallContext destructor (thiscall)
; ---------------------------------------------------------------
PUBLIC ??1HookCallContext@usvfs@@QAE@XZ
??1HookCallContext@usvfs@@QAE@XZ PROC
    push edi
    mov edi, ecx
    
    cmp byte ptr [edi+4], 0
    je _usvfsAsmHookCallContextSetLastError
    cmp dword ptr [edi+8], 0Ch
    je _usvfsAsmHookCallContextSetLastError
    
    call ?instance@HookStack@usvfs@@SAAEAV12@XZ
    mov ecx, eax
    mov edx, dword ptr [edi+8]
    call _usvfsAsmHookStackClearGroup
    
_usvfsAsmHookCallContextSetLastError:
    push dword ptr [edi]
    call dword ptr [__imp__SetLastError@4]
    pop edi
    ret
??1HookCallContext@usvfs@@QAE@XZ ENDP

; ---------------------------------------------------------------
; HookCallContext members (thiscall)
; ---------------------------------------------------------------
PUBLIC ?restoreLastError@HookCallContext@usvfs@@QAEXXZ
?restoreLastError@HookCallContext@usvfs@@QAEXXZ PROC
    push dword ptr [ecx]
    call dword ptr [__imp__SetLastError@4]
    ret
?restoreLastError@HookCallContext@usvfs@@QAEXXZ ENDP

PUBLIC ?updateLastError@HookCallContext@usvfs@@QAEXK@Z
?updateLastError@HookCallContext@usvfs@@QAEXK@Z PROC
    mov eax, dword ptr [esp+4]
    mov dword ptr [ecx], eax
    ret 4
?updateLastError@HookCallContext@usvfs@@QAEXK@Z ENDP

PUBLIC ?active@HookCallContext@usvfs@@QBE_NXZ
?active@HookCallContext@usvfs@@QBE_NXZ PROC
    movzx eax, byte ptr [ecx+4]
    ret
?active@HookCallContext@usvfs@@QBE_NXZ ENDP

; ---------------------------------------------------------------
; FunctionGroupLock constructors (thiscall)
; ---------------------------------------------------------------
PUBLIC ??0FunctionGroupLock@usvfs@@QAE@W4MutExHookGroup@1@@Z
??0FunctionGroupLock@usvfs@@QAE@W4MutExHookGroup@1@@Z PROC
    push ebp
    mov ebp, esp
    push edi
    push esi
    
    mov edi, ecx
    mov esi, dword ptr [ebp+8]
    mov dword ptr [edi], esi
    
    call ?instance@HookStack@usvfs@@SAAEAV12@XZ
    mov ecx, eax
    mov edx, esi
    call _usvfsAsmHookStackSetGroup
    
    mov byte ptr [edi+4], al
    mov eax, edi
    
    pop esi
    pop edi
    pop ebp
    ret 4
??0FunctionGroupLock@usvfs@@QAE@W4MutExHookGroup@1@@Z ENDP

PUBLIC ??1FunctionGroupLock@usvfs@@QAE@XZ
??1FunctionGroupLock@usvfs@@QAE@XZ PROC
    push edi
    mov edi, ecx
    cmp byte ptr [edi+4], 0
    je _usvfsAsmFunctionGroupLockDone
    
    call ?instance@HookStack@usvfs@@SAAEAV12@XZ
    mov ecx, eax
    mov edx, dword ptr [edi]
    call _usvfsAsmHookStackClearGroup
    
_usvfsAsmFunctionGroupLockDone:
    pop edi
    ret
??1FunctionGroupLock@usvfs@@QAE@XZ ENDP

; ---------------------------------------------------------------
; usvfs::operator<< (stdcall, mangled)
; ---------------------------------------------------------------
PUBLIC ??6usvfs@@YAAEAV?$basic_ostream@DU?$char_traits@D@std@@@std@@AEAV12@AEBURedirectionData@0@@Z
??6usvfs@@YAAEAV?$basic_ostream@DU?$char_traits@D@std@@@std@@AEAV12@AEBURedirectionData@0@@Z PROC
    ; Redirect to bridge impl (__cdecl)
    jmp _usvfsAsmStreamRedirectionDataImpl
??6usvfs@@YAAEAV?$basic_ostream@DU?$char_traits@D@std@@@std@@AEAV12@AEBURedirectionData@0@@Z ENDP

; ---------------------------------------------------------------
; RecursiveBenaphore (thiscall)
; ---------------------------------------------------------------
PUBLIC ??0RecursiveBenaphore@@QAE@XZ
??0RecursiveBenaphore@@QAE@XZ PROC
    push ebp
    mov ebp, esp
    push edi
    
    mov edi, ecx
    xor eax, eax
    mov dword ptr [edi], eax
    mov dword ptr [edi+4], eax
    mov dword ptr [edi+8], eax
    
    push 0 ; name
    push 1 ; max
    push 1 ; initial
    push 0 ; attr
    call dword ptr [__imp__CreateSemaphoreW@16]
    
    mov dword ptr [edi+10h], eax
    mov eax, edi
    
    pop edi
    pop ebp
    ret
??0RecursiveBenaphore@@QAE@XZ ENDP

PUBLIC ??1RecursiveBenaphore@@QAE@XZ
??1RecursiveBenaphore@@QAE@XZ PROC
    push dword ptr [ecx+10h]
    call dword ptr [__imp__CloseHandle@4]
    ret
??1RecursiveBenaphore@@QAE@XZ ENDP

PUBLIC ?wait@RecursiveBenaphore@@QAEXK@Z
?wait@RecursiveBenaphore@@QAEXK@Z PROC
    push ebp
    mov ebp, esp
    sub esp, 12 ; Local vars: threadId (4), stealCount (4), threadHandle (4)
    push esi
    push edi
    push ebx
    
    mov edi, ecx ; this
    mov esi, dword ptr [ebp+8] ; timeout
    
    call dword ptr [__imp__GetCurrentThreadId@0]
    mov dword ptr [ebp-4], eax ; threadId
    
    mov eax, 1
    lock xadd dword ptr [edi], eax
    inc eax ; eax was previous value
    cmp eax, 1
    jle _usvfsAsmRecursiveBenaphoreWaitDone
    
    mov ebx, dword ptr [ebp-4]
    cmp ebx, dword ptr [edi+4] ; owner thread
    je _usvfsAsmRecursiveBenaphoreWaitDone
    
    mov dword ptr [ebp-8], 3 ; stealCount
    
_usvfsAsmRecursiveBenaphoreWaitLoop:
    push esi ; timeout
    push dword ptr [edi+10h] ; sem
    call dword ptr [__imp__WaitForSingleObject@8]
    test eax, eax
    je _usvfsAsmRecursiveBenaphoreWaitDone
    
    ; Wait failed/timeout, check if owner died
    push dword ptr [edi+4] ; hThreadId
    push 0 ; bInheritHandle
    push 100000h ; THREAD_QUERY_LIMITED_INFORMATION
    call dword ptr [__imp__OpenThread@12]
    mov dword ptr [ebp-12], eax ; threadHandle
    
    cmp dword ptr [ebp-8], 0
    jle _usvfsAsmRecursiveBenaphoreSteal
    
    push 0 ; timeout
    push eax
    call dword ptr [__imp__WaitForSingleObject@8]
    test eax, eax
    je _usvfsAsmRecursiveBenaphoreSteal
    
    dec dword ptr [ebp-8]
    push dword ptr [ebp-12]
    call dword ptr [__imp__CloseHandle@4]
    jmp _usvfsAsmRecursiveBenaphoreWaitLoop
    
_usvfsAsmRecursiveBenaphoreSteal:
    mov dword ptr [edi+8], 0 ; recursion
    push dword ptr [edi+4] ; old owner
    call _usvfsAsmRecursiveBenaphoreOwnerDiedLogImpl
    add esp, 4
    push dword ptr [ebp-12]
    call dword ptr [__imp__CloseHandle@4]
    
_usvfsAsmRecursiveBenaphoreWaitDone:
    mov eax, dword ptr [ebp-4] ; threadId
    mov dword ptr [edi+4], eax
    inc dword ptr [edi+8] ; recursion++
    
    pop ebx
    pop edi
    pop esi
    mov esp, ebp
    pop ebp
    ret 4
?wait@RecursiveBenaphore@@QAEXK@Z ENDP

PUBLIC ?signal@RecursiveBenaphore@@QAEXXZ
?signal@RecursiveBenaphore@@QAEXXZ PROC
    mov edx, dword ptr [ecx+8] ; recursion
    test edx, edx
    je _usvfsAsmRecursiveBenaphoreSignalDone
    
    dec edx
    mov dword ptr [ecx+8], edx
    jne _usvfsAsmRecursiveBenaphoreSignalDecrement
    
    mov dword ptr [ecx+4], 0 ; owner = 0
    
_usvfsAsmRecursiveBenaphoreSignalDecrement:
    mov eax, 0FFFFFFFFh
    lock xadd dword ptr [ecx], eax
    cmp eax, 1
    je _usvfsAsmRecursiveBenaphoreSignalDone
    
    test edx, edx
    jne _usvfsAsmRecursiveBenaphoreSignalDone
    
    push 0 ; relCount
    push 1 ; count
    push dword ptr [ecx+10h] ; sem
    call dword ptr [__imp__ReleaseSemaphore@12]
    
_usvfsAsmRecursiveBenaphoreSignalDone:
    ret
?signal@RecursiveBenaphore@@QAEXXZ ENDP

END
