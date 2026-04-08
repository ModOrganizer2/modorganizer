OPTION CASEMAP:NONE

EXTERN __imp_GetProcessHeap:QWORD
EXTERN __imp_HeapAlloc:QWORD
EXTERN __imp_HeapFree:QWORD

USVFS_PARAMETERS_SIZE               EQU 464
USVFS_INSTANCE_NAME_OFFSET          EQU 0
USVFS_CURRENT_SHM_NAME_OFFSET       EQU 65
USVFS_CURRENT_INV_NAME_OFFSET       EQU 130
USVFS_DEBUG_MODE_OFFSET             EQU 195
USVFS_LOG_LEVEL_OFFSET              EQU 196
USVFS_CRASH_DUMPS_TYPE_OFFSET       EQU 197
USVFS_CRASH_DUMPS_PATH_OFFSET       EQU 198
USVFS_DELAY_PROCESS_MS_OFFSET       EQU 460
USVFS_PARAMETERS_QWORD_COUNT        EQU (USVFS_PARAMETERS_SIZE / 8)
USVFS_INSTANCE_NAME_SIZE            EQU 65
USVFS_CURRENT_SHM_NAME_SIZE         EQU 65
USVFS_CURRENT_SHM_TRUNCATE_SIZE     EQU 60
USVFS_CURRENT_INV_NAME_SIZE         EQU 65
USVFS_CURRENT_INV_TRUNCATE_SIZE     EQU 60
USVFS_CURRENT_INV_REMAINDER_SIZE    EQU (USVFS_CURRENT_INV_TRUNCATE_SIZE - 4)
USVFS_CRASH_DUMPS_PATH_SIZE         EQU 260

.DATA
logLevelDebug      DB "debug", 0
logLevelInfo       DB "info", 0
logLevelWarning    DB "warning", 0
logLevelError      DB "error", 0
logLevelUnknown    DB "unknown", 0

crashDumpNone      DB "none", 0
crashDumpMini      DB "mini", 0
crashDumpData      DB "data", 0
crashDumpFull      DB "full", 0
crashDumpUnknown   DB "unknown", 0

ALIGN 8
logLevelTable      QWORD logLevelDebug
                   QWORD logLevelInfo
                   QWORD logLevelWarning
                   QWORD logLevelError
LOG_LEVEL_TABLE_COUNT EQU 4

ALIGN 8
crashDumpTable     QWORD crashDumpNone
                   QWORD crashDumpMini
                   QWORD crashDumpData
                   QWORD crashDumpFull
CRASH_DUMP_TABLE_COUNT EQU 4

.CODE

; ---------------------------------------------------------------
; _usvfsAsmZeroBytes(rcx = dest, edx = count)
; Zeros 'count' bytes at 'dest'.  Uses rep stosb for efficiency.
; ---------------------------------------------------------------
_usvfsAsmZeroBytes PROC
    test edx, edx
    jle short _usvfsAsmZeroBytes_done

    push rdi
    mov rdi, rcx
    mov ecx, edx
    xor eax, eax
    rep stosb
    pop rdi

_usvfsAsmZeroBytes_done:
    ret
_usvfsAsmZeroBytes ENDP

; ---------------------------------------------------------------
; _usvfsAsmCopyTruncate(rcx = dest, edx = bufSize, r8 = src)
; Copies up to bufSize-1 chars from src to dest, null-terminates.
; ---------------------------------------------------------------
_usvfsAsmCopyTruncate PROC
    test edx, edx
    jle short _usvfsAsmCopyTruncate_done

    test r8, r8
    jz short _usvfsAsmCopyTruncate_zero

    lea r9d, [rdx-1]

_usvfsAsmCopyTruncate_loop:
    test r9d, r9d
    jle short _usvfsAsmCopyTruncate_terminate

    mov al, byte ptr [r8]
    inc r8
    mov byte ptr [rcx], al
    inc rcx
    test al, al
    je short _usvfsAsmCopyTruncate_done
    dec r9d
    jmp short _usvfsAsmCopyTruncate_loop

_usvfsAsmCopyTruncate_terminate:
    mov byte ptr [rcx], 0
    ret

_usvfsAsmCopyTruncate_zero:
    mov byte ptr [rcx], 0

_usvfsAsmCopyTruncate_done:
    ret
_usvfsAsmCopyTruncate ENDP

; ---------------------------------------------------------------
; _usvfsAsmStrnlenMax(rcx = str, edx = maxLen)
; Returns length of str capped at maxLen in eax.
; ---------------------------------------------------------------
_usvfsAsmStrnlenMax PROC
    xor eax, eax
    test rcx, rcx
    jz short _usvfsAsmStrnlenMax_done

_usvfsAsmStrnlenMax_loop:
    cmp eax, edx
    jae short _usvfsAsmStrnlenMax_done
    cmp byte ptr [rcx+rax], 0
    je short _usvfsAsmStrnlenMax_done
    inc eax
    jmp short _usvfsAsmStrnlenMax_loop

_usvfsAsmStrnlenMax_done:
    ret
_usvfsAsmStrnlenMax ENDP

; ---------------------------------------------------------------
; _usvfsAsmSetInstanceNameFields(rcx = params, rdx = name)
; Sets instanceName, currentSHMName and currentInverseSHMName.
; ---------------------------------------------------------------
_usvfsAsmSetInstanceNameFields PROC
    mov r10, rcx
    mov r11, rdx

    lea rcx, [r10 + USVFS_INSTANCE_NAME_OFFSET]
    mov edx, USVFS_INSTANCE_NAME_SIZE
    mov r8, r11
    call _usvfsAsmCopyTruncate

    lea rcx, [r10 + USVFS_CURRENT_SHM_NAME_OFFSET]
    mov edx, USVFS_CURRENT_SHM_TRUNCATE_SIZE
    mov r8, r11
    call _usvfsAsmCopyTruncate

    lea rcx, [r10 + USVFS_CURRENT_INV_NAME_OFFSET]
    mov edx, USVFS_CURRENT_INV_NAME_SIZE
    call _usvfsAsmZeroBytes

    lea rcx, [r10 + USVFS_CURRENT_INV_NAME_OFFSET]
    mov dword ptr [rcx], 5F766E69h
    lea rcx, [r10 + USVFS_CURRENT_INV_NAME_OFFSET + 4]
    mov edx, USVFS_CURRENT_INV_REMAINDER_SIZE
    mov r8, r11
    call _usvfsAsmCopyTruncate
    ret
_usvfsAsmSetInstanceNameFields ENDP

; ---------------------------------------------------------------
; usvfsParameters default constructor
; ---------------------------------------------------------------
PUBLIC ??0usvfsParameters@@QEAA@XZ
??0usvfsParameters@@QEAA@XZ PROC
    push rdi
    mov rax, rcx
    mov rdi, rcx
    xor eax, eax
    mov ecx, USVFS_PARAMETERS_QWORD_COUNT
    rep stosq
    mov rax, rdi
    sub rax, USVFS_PARAMETERS_SIZE
    pop rdi
    ret
??0usvfsParameters@@QEAA@XZ ENDP

; ---------------------------------------------------------------
; usvfsParameters parameterized constructor (10-arg form)
; ---------------------------------------------------------------
PUBLIC ??0usvfsParameters@@QEAA@PEBD00_NW4LogLevel@@W4CrashDumpsType@@0H@Z
??0usvfsParameters@@QEAA@PEBD00_NW4LogLevel@@W4CrashDumpsType@@0H@Z PROC FRAME
    sub rsp, 58h
    .allocstack 58h
    .endprolog

    mov qword ptr [rsp+20h], rcx
    mov qword ptr [rsp+28h], rdx
    mov qword ptr [rsp+30h], r8
    mov qword ptr [rsp+38h], r9

    call ??0usvfsParameters@@QEAA@XZ

    mov r10, qword ptr [rsp+20h]

    lea rcx, [r10 + USVFS_INSTANCE_NAME_OFFSET]
    mov edx, USVFS_INSTANCE_NAME_SIZE
    mov r8, qword ptr [rsp+28h]
    call _usvfsAsmCopyTruncate

    lea rcx, [r10 + USVFS_CURRENT_SHM_NAME_OFFSET]
    mov edx, USVFS_CURRENT_SHM_NAME_SIZE
    mov r8, qword ptr [rsp+30h]
    call _usvfsAsmCopyTruncate

    lea rcx, [r10 + USVFS_CURRENT_INV_NAME_OFFSET]
    mov edx, USVFS_CURRENT_INV_NAME_SIZE
    mov r8, qword ptr [rsp+38h]
    call _usvfsAsmCopyTruncate

    mov r10, qword ptr [rsp+20h]
    mov al, byte ptr [rsp+80h]
    mov byte ptr [r10 + USVFS_DEBUG_MODE_OFFSET], al
    mov al, byte ptr [rsp+88h]
    mov byte ptr [r10 + USVFS_LOG_LEVEL_OFFSET], al
    mov al, byte ptr [rsp+90h]
    mov byte ptr [r10 + USVFS_CRASH_DUMPS_TYPE_OFFSET], al

    lea rcx, [r10 + USVFS_CRASH_DUMPS_PATH_OFFSET]
    mov edx, USVFS_CRASH_DUMPS_PATH_SIZE
    mov r8, qword ptr [rsp+98h]
    call _usvfsAsmCopyTruncate

    mov rcx, qword ptr [rsp+20h]
    mov eax, dword ptr [rsp+0A0h]
    mov dword ptr [rcx + USVFS_DELAY_PROCESS_MS_OFFSET], eax
    mov rax, rcx

    add rsp, 58h
    ret
??0usvfsParameters@@QEAA@PEBD00_NW4LogLevel@@W4CrashDumpsType@@0H@Z ENDP

; ---------------------------------------------------------------
; usvfsParameters conversion constructor (from USVFSParameters)
; ---------------------------------------------------------------
PUBLIC ??0usvfsParameters@@QEAA@AEBUUSVFSParameters@@@Z
??0usvfsParameters@@QEAA@AEBUUSVFSParameters@@@Z PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog

    mov qword ptr [rsp+20h], rcx
    mov qword ptr [rsp+28h], rdx

    call ??0usvfsParameters@@QEAA@XZ

    mov r10, qword ptr [rsp+20h]
    mov r11, qword ptr [rsp+28h]

    lea rcx, [r10 + USVFS_INSTANCE_NAME_OFFSET]
    mov edx, USVFS_INSTANCE_NAME_SIZE
    mov r8, r11
    call _usvfsAsmCopyTruncate

    lea rcx, [r10 + USVFS_CURRENT_SHM_NAME_OFFSET]
    mov edx, USVFS_CURRENT_SHM_NAME_SIZE
    lea r8, [r11 + USVFS_CURRENT_SHM_NAME_OFFSET]
    call _usvfsAsmCopyTruncate

    lea rcx, [r10 + USVFS_CURRENT_INV_NAME_OFFSET]
    mov edx, USVFS_CURRENT_INV_NAME_SIZE
    lea r8, [r11 + USVFS_CURRENT_INV_NAME_OFFSET]
    call _usvfsAsmCopyTruncate

    mov r10, qword ptr [rsp+20h]
    mov r11, qword ptr [rsp+28h]
    mov al, byte ptr [r11 + USVFS_DEBUG_MODE_OFFSET]
    mov byte ptr [r10 + USVFS_DEBUG_MODE_OFFSET], al
    mov al, byte ptr [r11 + USVFS_LOG_LEVEL_OFFSET]
    mov byte ptr [r10 + USVFS_LOG_LEVEL_OFFSET], al
    mov al, byte ptr [r11 + USVFS_CRASH_DUMPS_TYPE_OFFSET]
    mov byte ptr [r10 + USVFS_CRASH_DUMPS_TYPE_OFFSET], al

    lea rcx, [r10 + USVFS_CRASH_DUMPS_PATH_OFFSET]
    mov edx, USVFS_CRASH_DUMPS_PATH_SIZE
    lea r8, [r11 + USVFS_CRASH_DUMPS_PATH_OFFSET]
    call _usvfsAsmCopyTruncate

    mov rcx, qword ptr [rsp+20h]
    xor edx, edx
    mov dword ptr [rcx + USVFS_DELAY_PROCESS_MS_OFFSET], edx
    mov rax, rcx

    add rsp, 38h
    ret
??0usvfsParameters@@QEAA@AEBUUSVFSParameters@@@Z ENDP

; ---------------------------------------------------------------
; usvfsCreateParameters — heap-allocate a zeroed usvfsParameters
; ---------------------------------------------------------------
PUBLIC usvfsCreateParameters
usvfsCreateParameters PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    call qword ptr [__imp_GetProcessHeap]
    mov rcx, rax
    mov edx, 8
    mov r8d, USVFS_PARAMETERS_SIZE
    call qword ptr [__imp_HeapAlloc]

    add rsp, 28h
    ret
usvfsCreateParameters ENDP

; ---------------------------------------------------------------
; usvfsDupeParameters — duplicate a usvfsParameters via heap copy
; Uses rep movsq for fast 464-byte bulk copy.
; ---------------------------------------------------------------
PUBLIC usvfsDupeParameters
usvfsDupeParameters PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    test rcx, rcx
    jz short usvfsDupeParameters_null

    mov rbx, rcx
    call usvfsCreateParameters
    test rax, rax
    jz short usvfsDupeParameters_cleanup

    mov rdi, rax
    mov rsi, rbx
    mov ecx, USVFS_PARAMETERS_QWORD_COUNT
    rep movsq
    ; rax still points to start of dest (set before rep)
    ; rep movsq advances rdi, so restore rax
    sub rdi, USVFS_PARAMETERS_SIZE
    mov rax, rdi

usvfsDupeParameters_cleanup:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret

usvfsDupeParameters_null:
    xor eax, eax
    jmp short usvfsDupeParameters_cleanup
usvfsDupeParameters ENDP

; ---------------------------------------------------------------
; usvfsCopyParameters — copy usvfsParameters src to dest
; rcx = src, rdx = dest.  Uses rep movsq for fast bulk copy.
; ---------------------------------------------------------------
PUBLIC usvfsCopyParameters
usvfsCopyParameters PROC
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
    mov ecx, USVFS_PARAMETERS_QWORD_COUNT
    rep movsq
    pop rdi
    pop rsi
    ret
usvfsCopyParameters ENDP

; ---------------------------------------------------------------
; usvfsFreeParameters — free a heap-allocated usvfsParameters
; ---------------------------------------------------------------
PUBLIC usvfsFreeParameters
usvfsFreeParameters PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog

    test rcx, rcx
    jz short usvfsFreeParameters_done

    mov qword ptr [rsp+20h], rcx
    call qword ptr [__imp_GetProcessHeap]
    mov rcx, rax
    xor edx, edx
    mov r8, qword ptr [rsp+20h]
    call qword ptr [__imp_HeapFree]

usvfsFreeParameters_done:
    add rsp, 38h
    ret
usvfsFreeParameters ENDP

PUBLIC usvfsSetInstanceName
usvfsSetInstanceName PROC
    test rcx, rcx
    jz short usvfsSetInstanceName_done

    jmp ?setInstanceName@usvfsParameters@@QEAAXPEBD@Z

usvfsSetInstanceName_done:
    ret
usvfsSetInstanceName ENDP

PUBLIC usvfsSetDebugMode
usvfsSetDebugMode PROC
    test rcx, rcx
    jz short usvfsSetDebugMode_done

    test edx, edx
    setne al
    mov byte ptr [rcx + USVFS_DEBUG_MODE_OFFSET], al

usvfsSetDebugMode_done:
    ret
usvfsSetDebugMode ENDP

PUBLIC usvfsSetLogLevel
usvfsSetLogLevel PROC
    test rcx, rcx
    jz short usvfsSetLogLevel_done

    mov byte ptr [rcx + USVFS_LOG_LEVEL_OFFSET], dl

usvfsSetLogLevel_done:
    ret
usvfsSetLogLevel ENDP

PUBLIC usvfsSetCrashDumpType
usvfsSetCrashDumpType PROC
    test rcx, rcx
    jz short usvfsSetCrashDumpType_done

    mov byte ptr [rcx + USVFS_CRASH_DUMPS_TYPE_OFFSET], dl

usvfsSetCrashDumpType_done:
    ret
usvfsSetCrashDumpType ENDP

PUBLIC usvfsSetCrashDumpPath
usvfsSetCrashDumpPath PROC
    test rcx, rcx
    jz short usvfsSetCrashDumpPath_done

    jmp ?setCrashDumpPath@usvfsParameters@@QEAAXPEBD@Z

usvfsSetCrashDumpPath_done:
    ret
usvfsSetCrashDumpPath ENDP

PUBLIC usvfsSetProcessDelay
usvfsSetProcessDelay PROC
    test rcx, rcx
    jz short usvfsSetProcessDelay_done

    mov dword ptr [rcx + USVFS_DELAY_PROCESS_MS_OFFSET], edx

usvfsSetProcessDelay_done:
    ret
usvfsSetProcessDelay ENDP

; ---------------------------------------------------------------
; usvfsLogLevelToString — jump-table based O(1) dispatch
; ---------------------------------------------------------------
PUBLIC usvfsLogLevelToString
usvfsLogLevelToString PROC
    cmp ecx, LOG_LEVEL_TABLE_COUNT
    jae short usvfsLogLevelToString_unknown
    movsxd rax, ecx
    lea rdx, logLevelTable
    mov rax, qword ptr [rdx + rax*8]
    ret

usvfsLogLevelToString_unknown:
    lea rax, logLevelUnknown
    ret
usvfsLogLevelToString ENDP

; ---------------------------------------------------------------
; usvfsCrashDumpTypeToString — jump-table based O(1) dispatch
; ---------------------------------------------------------------
PUBLIC usvfsCrashDumpTypeToString
usvfsCrashDumpTypeToString PROC
    cmp ecx, CRASH_DUMP_TABLE_COUNT
    jae short usvfsCrashDumpTypeToString_unknown
    movsxd rax, ecx
    lea rdx, crashDumpTable
    mov rax, qword ptr [rdx + rax*8]
    ret

usvfsCrashDumpTypeToString_unknown:
    lea rax, crashDumpUnknown
    ret
usvfsCrashDumpTypeToString ENDP

PUBLIC ?setInstanceName@usvfsParameters@@QEAAXPEBD@Z
?setInstanceName@usvfsParameters@@QEAAXPEBD@Z PROC
    jmp _usvfsAsmSetInstanceNameFields
?setInstanceName@usvfsParameters@@QEAAXPEBD@Z ENDP

PUBLIC ?setDebugMode@usvfsParameters@@QEAAX_N@Z
?setDebugMode@usvfsParameters@@QEAAX_N@Z PROC
    mov byte ptr [rcx + USVFS_DEBUG_MODE_OFFSET], dl
    ret
?setDebugMode@usvfsParameters@@QEAAX_N@Z ENDP

PUBLIC ?setLogLevel@usvfsParameters@@QEAAXW4LogLevel@@@Z
?setLogLevel@usvfsParameters@@QEAAXW4LogLevel@@@Z PROC
    mov byte ptr [rcx + USVFS_LOG_LEVEL_OFFSET], dl
    ret
?setLogLevel@usvfsParameters@@QEAAXW4LogLevel@@@Z ENDP

PUBLIC ?setCrashDumpType@usvfsParameters@@QEAAXW4CrashDumpsType@@@Z
?setCrashDumpType@usvfsParameters@@QEAAXW4CrashDumpsType@@@Z PROC
    mov byte ptr [rcx + USVFS_CRASH_DUMPS_TYPE_OFFSET], dl
    ret
?setCrashDumpType@usvfsParameters@@QEAAXW4CrashDumpsType@@@Z ENDP

; ---------------------------------------------------------------
; setCrashDumpPath member function
; BUG FIX: the original code lost the source pointer (r8) after
; calling _usvfsAsmStrnlenMax because r8 is volatile.  We now save
; the source pointer in [rsp+28h] and reload it before passing to
; _usvfsAsmCopyTruncate.
; ---------------------------------------------------------------
PUBLIC ?setCrashDumpPath@usvfsParameters@@QEAAXPEBD@Z
?setCrashDumpPath@usvfsParameters@@QEAAXPEBD@Z PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog

    mov qword ptr [rsp+20h], rcx          ; save this (params ptr)
    mov qword ptr [rsp+28h], rdx          ; save source path ptr

    test rdx, rdx
    jz short usvfsSetCrashDumpPath_member_fail
    cmp byte ptr [rdx], 0
    je short usvfsSetCrashDumpPath_member_fail

    mov rcx, rdx
    mov edx, USVFS_CRASH_DUMPS_PATH_SIZE
    call _usvfsAsmStrnlenMax
    cmp eax, USVFS_CRASH_DUMPS_PATH_SIZE
    jae short usvfsSetCrashDumpPath_member_fail

    mov rcx, qword ptr [rsp+20h]
    lea rcx, [rcx + USVFS_CRASH_DUMPS_PATH_OFFSET]
    mov edx, USVFS_CRASH_DUMPS_PATH_SIZE
    mov r8, qword ptr [rsp+28h]           ; reload source (was lost)
    call _usvfsAsmCopyTruncate
    jmp short usvfsSetCrashDumpPath_member_done

usvfsSetCrashDumpPath_member_fail:
    mov rcx, qword ptr [rsp+20h]
    mov byte ptr [rcx + USVFS_CRASH_DUMPS_PATH_OFFSET], 0
    mov byte ptr [rcx + USVFS_CRASH_DUMPS_TYPE_OFFSET], 0

usvfsSetCrashDumpPath_member_done:
    add rsp, 38h
    ret
?setCrashDumpPath@usvfsParameters@@QEAAXPEBD@Z ENDP

PUBLIC ?setProcessDelay@usvfsParameters@@QEAAXH@Z
?setProcessDelay@usvfsParameters@@QEAAXH@Z PROC
    mov dword ptr [rcx + USVFS_DELAY_PROCESS_MS_OFFSET], edx
    ret
?setProcessDelay@usvfsParameters@@QEAAXH@Z ENDP

END
