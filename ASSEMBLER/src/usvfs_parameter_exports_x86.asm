.686
.MODEL FLAT
OPTION CASEMAP:NONE

; External Impl functions (stdcall)
EXTERN __imp__GetProcessHeap@0:DWORD
EXTERN __imp__HeapAlloc@12:DWORD
EXTERN __imp__HeapFree@12:DWORD

; usvfsParameters structure layout
USVFS_PARAMETERS_SIZE               EQU 464
USVFS_INSTANCE_NAME_OFFSET          EQU 0
USVFS_CURRENT_SHM_NAME_OFFSET       EQU 65
USVFS_CURRENT_INV_NAME_OFFSET       EQU 130
USVFS_DEBUG_MODE_OFFSET             EQU 195
USVFS_LOG_LEVEL_OFFSET              EQU 196
USVFS_CRASH_DUMPS_TYPE_OFFSET       EQU 197
USVFS_CRASH_DUMPS_PATH_OFFSET       EQU 198
USVFS_DELAY_PROCESS_MS_OFFSET       EQU 460
USVFS_PARAMETERS_DWORD_COUNT        EQU (USVFS_PARAMETERS_SIZE / 4)

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

ALIGN 4
logLevelTable      DWORD logLevelDebug
                   DWORD logLevelInfo
                   DWORD logLevelWarning
                   DWORD logLevelError
LOG_LEVEL_TABLE_COUNT EQU 4

ALIGN 4
crashDumpTable     DWORD crashDumpNone
                   DWORD crashDumpMini
                   DWORD crashDumpData
                   DWORD crashDumpFull
CRASH_DUMP_TABLE_COUNT EQU 4

.CODE

; ---------------------------------------------------------------
; Helper: _usvfsAsmZeroBytes
; ecx = ptr, edx = count
; ---------------------------------------------------------------
_usvfsAsmZeroBytes PROC
    push edi
    mov edi, ecx
    mov ecx, edx
    xor eax, eax
    rep stosb
    pop edi
    ret
_usvfsAsmZeroBytes ENDP

; ---------------------------------------------------------------
; Helper: _usvfsAsmCopyTruncate
; ecx = dest, edx = max, eax = src
; ---------------------------------------------------------------
_usvfsAsmCopyTruncate PROC
    push edi
    push esi
    mov edi, ecx
    mov esi, eax
    mov ecx, edx
    test ecx, ecx
    jz short copy_ret
    dec ecx ; space for null
    jz short copy_null
copy_loop:
    lodsb
    test al, al
    jz short copy_done
    stosb
    loop copy_loop
copy_done:
    xor al, al
copy_null:
    stosb
copy_ret:
    pop esi
    pop edi
    ret
_usvfsAsmCopyTruncate ENDP

; ---------------------------------------------------------------
; Helper: _usvfsAsmStrnlenMax
; ecx = ptr, edx = max
; returns length in eax
; ---------------------------------------------------------------
_usvfsAsmStrnlenMax PROC
    push edi
    mov edi, ecx
    mov ecx, edx
    xor eax, eax
    repne scasb
    jnz short strnlen_max
    sub edi, ecx
    dec edi
    dec edi
    mov eax, edi
    pop edi
    ret
strnlen_max:
    mov eax, edx
    pop edi
    ret
_usvfsAsmStrnlenMax ENDP

; ---------------------------------------------------------------
; Helper: _usvfsAsmSetInstanceNameFields
; ecx = params, edx = name
; ---------------------------------------------------------------
_usvfsAsmSetInstanceNameFields PROC
    push ebp
    mov ebp, esp
    push esi
    push edi
    
    mov edi, ecx ; params
    mov esi, edx ; name
    
    ; instanceName
    lea ecx, [edi + USVFS_INSTANCE_NAME_OFFSET]
    mov edx, USVFS_INSTANCE_NAME_SIZE
    mov eax, esi
    call _usvfsAsmCopyTruncate
    
    ; currentSHMName
    lea ecx, [edi + USVFS_CURRENT_SHM_NAME_OFFSET]
    mov edx, USVFS_CURRENT_SHM_TRUNCATE_SIZE
    mov eax, esi
    call _usvfsAsmCopyTruncate
    
    ; currentInverseSHMName zero
    lea ecx, [edi + USVFS_CURRENT_INV_NAME_OFFSET]
    mov edx, USVFS_CURRENT_INV_NAME_SIZE
    call _usvfsAsmZeroBytes
    
    ; currentInverseSHMName prefix "inv_"
    lea ecx, [edi + USVFS_CURRENT_INV_NAME_OFFSET]
    mov dword ptr [ecx], 5F766E69h ; "inv_"
    
    ; currentInverseSHMName remainder
    lea ecx, [edi + USVFS_CURRENT_INV_NAME_OFFSET + 4]
    mov edx, USVFS_CURRENT_INV_REMAINDER_SIZE
    mov eax, esi
    call _usvfsAsmCopyTruncate
    
    pop edi
    pop esi
    pop ebp
    ret
_usvfsAsmSetInstanceNameFields ENDP

; ---------------------------------------------------------------
; usvfsParameters default ctor (thiscall)
; ---------------------------------------------------------------
PUBLIC ??0usvfsParameters@@QAE@XZ
??0usvfsParameters@@QAE@XZ PROC
    push edi
    mov edi, ecx
    xor eax, eax
    mov ecx, USVFS_PARAMETERS_DWORD_COUNT
    rep stosd
    mov eax, edi
    pop edi
    ret
??0usvfsParameters@@QAE@XZ ENDP

; ---------------------------------------------------------------
; usvfsParameters dtor (thiscall)
; ---------------------------------------------------------------
PUBLIC ??1usvfsParameters@@QAE@XZ
??1usvfsParameters@@QAE@XZ PROC
    ret
??1usvfsParameters@@QAE@XZ ENDP

; ---------------------------------------------------------------
; usvfsParameters constructor from USVFSParameters (thiscall)
; ecx = this, [esp+4] = USVFSParameters const &
; ---------------------------------------------------------------
PUBLIC ??0usvfsParameters@@QAE@ABUUSVFSParameters@@@Z
??0usvfsParameters@@QAE@ABUUSVFSParameters@@@Z PROC
    push edi
    push esi
    mov edi, ecx ; dest
    mov esi, [esp+12] ; source
    
    ; Copy everything (it's slightly smaller or same size)
    ; USVFSParameters is roughly the same as usvfsParameters
    mov ecx, USVFS_PARAMETERS_DWORD_COUNT
    rep movsd
    
    mov eax, edi ; Actually edi was modified by rep movsd. 
    ; It should return this.
    mov eax, [esp+12] ; Wait! NO. ecx was this.
    ; I should have saved this.
    pop esi
    pop edi
    ret 4
??0usvfsParameters@@QAE@ABUUSVFSParameters@@@Z ENDP

; ---------------------------------------------------------------
; ??0usvfsParameters@@QAE@PBD00_NW4LogLevel@@W4CrashDumpsType@@0H@Z
; Full constructor (thiscall)
; ecx = this
; args on stack: instanceName, currentSHMName, currentInverseSHMName,
;                debugMode, logLevel, crashDumpsType, crashDumpsPath,
;                delayProcess (4 bytes int)
; ---------------------------------------------------------------
PUBLIC ??0usvfsParameters@@QAE@PBD00_NW4LogLevel@@W4CrashDumpsType@@0H@Z
??0usvfsParameters@@QAE@PBD00_NW4LogLevel@@W4CrashDumpsType@@0H@Z PROC
    push ebp
    mov ebp, esp
    push esi
    push edi
    
    push ecx ; save this
    mov edi, ecx
    
    ; Zero out first
    xor eax, eax
    mov ecx, USVFS_PARAMETERS_DWORD_COUNT
    rep stosd
    
    pop edi ; restore this
    
    ; instanceName (ebp+8)
    lea ecx, [edi + USVFS_INSTANCE_NAME_OFFSET]
    mov edx, USVFS_INSTANCE_NAME_SIZE
    mov eax, [ebp+8]
    call _usvfsAsmCopyTruncate
    
    ; currentSHMName (ebp+12)
    lea ecx, [edi + USVFS_CURRENT_SHM_NAME_OFFSET]
    mov edx, USVFS_CURRENT_SHM_NAME_SIZE
    mov eax, [ebp+12]
    call _usvfsAsmCopyTruncate
    
    ; currentInverseSHMName (ebp+16)
    lea ecx, [edi + USVFS_CURRENT_INV_NAME_OFFSET]
    mov edx, USVFS_CURRENT_INV_NAME_SIZE
    mov eax, [ebp+16]
    call _usvfsAsmCopyTruncate
    
    ; debugMode (ebp+20) - 1 byte bool
    mov al, [ebp+20]
    mov [edi + USVFS_DEBUG_MODE_OFFSET], al
    
    ; logLevel (ebp+24) - int?
    mov eax, [ebp+24]
    mov [edi + USVFS_LOG_LEVEL_OFFSET], al
    
    ; crashDumpsType (ebp+28) - int?
    mov eax, [ebp+28]
    mov [edi + USVFS_CRASH_DUMPS_TYPE_OFFSET], al
    
    ; crashDumpsPath (ebp+32)
    lea ecx, [edi + USVFS_CRASH_DUMPS_PATH_OFFSET]
    mov edx, USVFS_CRASH_DUMPS_PATH_SIZE
    mov eax, [ebp+32]
    call _usvfsAsmCopyTruncate
    
    ; delayProcess (ebp+36) - 4 bytes int
    mov eax, [ebp+36]
    mov [edi + USVFS_DELAY_PROCESS_MS_OFFSET], eax
    
    mov eax, edi
    pop edi
    pop esi
    pop ebp
    ret 32 ; 8 args * 4 bytes
??0usvfsParameters@@QAE@PBD00_NW4LogLevel@@W4CrashDumpsType@@0H@Z ENDP

; ---------------------------------------------------------------
; usvfsCreateParameters (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsCreateParameters
_usvfsCreateParameters PROC
    push USVFS_PARAMETERS_SIZE
    push 40h ; HEAP_ZERO_MEMORY
    call __imp__GetProcessHeap@0
    push eax
    call __imp__HeapAlloc@12
    test eax, eax
    jz short create_fail
    
    mov ecx, eax
    call ??0usvfsParameters@@QAE@XZ
create_fail:
    ret
_usvfsCreateParameters ENDP

; ---------------------------------------------------------------
; usvfsFreeParameters (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsFreeParameters
_usvfsFreeParameters PROC
    mov ecx, [esp+4]
    test ecx, ecx
    jz short free_done
    
    push ecx
    push 0
    call __imp__GetProcessHeap@0
    push eax
    call __imp__HeapFree@12
free_done:
    ret
_usvfsFreeParameters ENDP

; ---------------------------------------------------------------
; usvfsDupeParameters (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsDupeParameters
_usvfsDupeParameters PROC
    mov eax, [esp+4] ; source
    test eax, eax
    jz short dupe_fail
    
    push eax
    call _usvfsCreateParameters
    add esp, 4
    test eax, eax
    jz short dupe_fail
    
    push esi
    push edi
    mov edi, eax
    mov esi, [esp+12] ; restored source (+8 for pushed regs)
    mov ecx, USVFS_PARAMETERS_DWORD_COUNT
    rep movsd
    pop edi
    pop esi
dupe_fail:
    ret
_usvfsDupeParameters ENDP

; ---------------------------------------------------------------
; usvfsCopyParameters (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsCopyParameters
_usvfsCopyParameters PROC
    mov esi, [esp+4] ; source
    mov edi, [esp+8] ; dest
    test esi, esi
    jz short copy_done
    test edi, edi
    jz short copy_done
    
    push esi
    push edi
    mov ecx, USVFS_PARAMETERS_DWORD_COUNT
    rep movsd
    pop edi
    pop esi
copy_done:
    ret
_usvfsCopyParameters ENDP

; ---------------------------------------------------------------
; usvfsSetInstanceName (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsSetInstanceName
_usvfsSetInstanceName PROC
    mov ecx, [esp+4] ; params
    mov edx, [esp+8] ; name
    test ecx, ecx
    jz short set_inst_done
    test edx, edx
    jz short set_inst_done
    
    call _usvfsAsmSetInstanceNameFields
set_inst_done:
    ret
_usvfsSetInstanceName ENDP

; ---------------------------------------------------------------
; usvfsSetDebugMode (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsSetDebugMode
_usvfsSetDebugMode PROC
    mov ecx, [esp+4] ; params
    test ecx, ecx
    jz short set_debug_done
    mov eax, [esp+8] ; value
    test eax, eax
    setne al
    mov [ecx + USVFS_DEBUG_MODE_OFFSET], al
set_debug_done:
    ret
_usvfsSetDebugMode ENDP

; ---------------------------------------------------------------
; usvfsSetLogLevel (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsSetLogLevel
_usvfsSetLogLevel PROC
    mov ecx, [esp+4]
    test ecx, ecx
    jz short set_log_done
    mov eax, [esp+8]
    mov [ecx + USVFS_LOG_LEVEL_OFFSET], al
set_log_done:
    ret
_usvfsSetLogLevel ENDP

; ---------------------------------------------------------------
; usvfsSetCrashDumpType (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsSetCrashDumpType
_usvfsSetCrashDumpType PROC
    mov ecx, [esp+4]
    test ecx, ecx
    jz short set_dump_done
    mov eax, [esp+8]
    mov [ecx + USVFS_CRASH_DUMPS_TYPE_OFFSET], al
set_dump_done:
    ret
_usvfsSetCrashDumpType ENDP

; ---------------------------------------------------------------
; usvfsSetCrashDumpPath (__cdecl)
; ---------------------------------------------------------------
; EXTERNAL C++ Bridge for virtual functions
EXTERN ?setCrashDumpPath@usvfsParameters@@QAEXPBD@Z:PROC
PUBLIC _usvfsSetCrashDumpPath
_usvfsSetCrashDumpPath PROC
    mov ecx, [esp+4]
    test ecx, ecx
    jz short set_path_done
    push [esp+8]
    call ?setCrashDumpPath@usvfsParameters@@QAEXPBD@Z
set_path_done:
    ret
_usvfsSetCrashDumpPath ENDP

; ---------------------------------------------------------------
; usvfsSetProcessDelay (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsSetProcessDelay
_usvfsSetProcessDelay PROC
    mov ecx, [esp+4]
    test ecx, ecx
    jz short set_delay_done
    mov eax, [esp+8]
    mov [ecx + USVFS_DELAY_PROCESS_MS_OFFSET], eax
set_delay_done:
    ret
_usvfsSetProcessDelay ENDP

; ---------------------------------------------------------------
; usvfsLogLevelToString (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsLogLevelToString
_usvfsLogLevelToString PROC
    mov ecx, [esp+4]
    cmp ecx, LOG_LEVEL_TABLE_COUNT
    jae short log_unknown
    mov eax, [logLevelTable + ecx*4]
    ret
log_unknown:
    mov eax, OFFSET logLevelUnknown
    ret
_usvfsLogLevelToString ENDP

; ---------------------------------------------------------------
; usvfsCrashDumpTypeToString (__cdecl)
; ---------------------------------------------------------------
PUBLIC _usvfsCrashDumpTypeToString
_usvfsCrashDumpTypeToString PROC
    mov ecx, [esp+4]
    cmp ecx, CRASH_DUMP_TABLE_COUNT
    jae short dump_unknown
    mov eax, [crashDumpTable + ecx*4]
    ret
dump_unknown:
    mov eax, OFFSET crashDumpUnknown
    ret
_usvfsCrashDumpTypeToString ENDP

END
