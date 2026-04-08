# USVFS x64 Assembler Rewrite

This directory hosts the assembler rewrite layer for the real `usvfs_x64`
source used by Mod Organizer 2 `v2.4.4`.

The upstream library source itself is not vendored here. Instead, this folder
contains:

- x64 MASM assembly exports that replace the entire `usvfs` exported API surface
- C++ bridge files that implement the complex logic behind each assembly export
- patch files that teach the upstream `0.5.6.0` source tree to consume the
  assembly files
- bootstrap/build scripts that clone the exact upstream revision, apply the
  patch, and build the x64 DLL

## Port Status — Complete

The full exported x64 C API surface is now assembler-fronted. All 101 exports
(40 C-linkage + 61 C++ mangled symbols) are pinned in `src/usvfs_x64.def` to
match the shipped MO2 `v2.4.4` binary exactly.

### Assembly Export Files

| File | Exports | Description |
|------|---------|-------------|
| `usvfs_parameter_exports_x64.asm` | 12 | `usvfsParameters` struct operations, create/dupe/copy/free, setter/getter helpers, enum-to-string converters |
| `usvfs_exports_x64.asm` | 28 | Lifecycle & API trampolines: `CreateVFS`, `ConnectVFS`, `DisconnectVFS`, `VirtualLinkFile`, `VirtualLinkDirectoryStatic`, `CreateProcessHooked`, etc. |
| `usvfs_runtime_x64.asm` | — | Internal class implementations: `HookCallContext`, `FunctionGroupLock`, `HookStack` (TLS), `RecursiveBenaphore` (mutex), `RedirectionData` stream operator |
| `usvfs_context_x64.asm` | — | Internal class trampolines: `HookContext` ctor/dtor/members, `HookManager` ctor/dtor/members |

### C++ Bridge Files

| File | Purpose |
|------|---------|
| `usvfs_exports_bridge.cpp` | Full implementations for all lifecycle exports (logging, VFS connect, process hooking, file linking, crash dumps, etc.) plus `DllMain` |
| `usvfs_context_bridge.cpp` | Full `HookContext` and `HookManager` implementations via size-validated proxies, `SharedParameters` (interprocess), hook installation |
| `usvfs_kernel32_bridge.cpp` | Pass-through include of upstream `hooks/kernel32.cpp` with assembly macro disabled |
| `usvfs_ntdll_bridge.cpp` | Pass-through include of upstream `hooks/ntdll.cpp` with assembly macro disabled |

### Bug Fixes Applied

- **`setCrashDumpPath` r8 clobber**: The source path pointer (`r8`) was lost
  after calling `usvfsAsmStrnlenMax` because `r8` is a volatile register in
  the Windows x64 ABI. The source pointer is now saved in a stack slot and
  reloaded before the copy.

- **`usvfsAsmCopyTruncate` dead code**: Removed unreachable `js` instruction
  that could never trigger (LEA does not set processor flags).

### Performance Optimizations

- `usvfsAsmZeroBytes` — replaced byte-by-byte loop with `rep stosb`
- `usvfsDupeParameters` / `usvfsCopyParameters` — replaced qword loop with
  `rep movsq` for fast 464-byte bulk copy
- `usvfsLogLevelToString` / `usvfsCrashDumpTypeToString` — replaced branchy
  compare chains with data-table O(1) dispatch
- Default constructor — uses `rep stosq` for zeroing

## Upstream Baseline

The target baseline is `ModOrganizer2/usvfs` commit `7368b25`, which reports
`USVFS_VERSION_STRING == 0.5.6.0` and matches the shipped `usvfs_x64.dll`
version used by MO2 `v2.4.4`.

## Architecture

```
Exported MASM x64 Stubs
    ↓ (jmp / call)
C++ Bridge Implementations
    ↓ (include)
Upstream usvfs 0.5.6.0 Types & Headers
    ↓
Windows NT API (kernel32, ntdll, Boost.Interprocess)
```

## Drop-in Replacement

The built `usvfs_x64.dll` is a binary-compatible drop-in replacement for the
shipped MO2 `v2.4.4` DLL. To replace:

1. Build using the instructions below
2. Copy the output `usvfs_x64.dll` over `install/bin/usvfs_x64.dll`
3. Copy the output `usvfs_x64.lib` over `install/libs/usvfs_x64.lib`

## Reproducible Build Flow

Prepare a clean upstream tree:

```powershell
.\ASSEMBLER\prepare-usvfs-source.ps1 -UseVcpkgBoost
```

Build the x64 DLL:

```powershell
.\ASSEMBLER\build-usvfs-x64.ps1 -UseVcpkgBoost
```

The prep step:

- clones `ModOrganizer2/usvfs`
- checks out commit `7368b25`
- initializes submodules
- applies `ASSEMBLER/patches/usvfs-0.5.6.0-asm-parameter-exports.patch`
- optionally creates a Boost compatibility shim from a `vcpkg` install

The GitHub Actions build currently expects the `vcpkg` Boost modules used by the
upstream codebase, including `algorithm`, `any`, `container`, `date-time`,
`dll`, `exception`, `filesystem`, `format`, `interprocess`, `lexical-cast`,
`locale`, `multi-index`, `thread`, and `tokenizer`.
