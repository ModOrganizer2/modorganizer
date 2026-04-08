[CmdletBinding()]
param(
    [string]$MO2Version = '2.4.4',
    [string]$SourceDir,
    [string]$RepoUrl = 'https://github.com/ModOrganizer2/usvfs.git',
    [string]$Commit,
    [string]$BoostPath,
    [switch]$UseVcpkgBoost,
    [string]$VcpkgRoot = 'C:\vcpkg',
    [string]$Triplet = 'x64-windows-static'
)

if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = (Join-Path $PSScriptRoot "..\external\usvfs-mo2-v$MO2Version")
}
if ([string]::IsNullOrWhiteSpace($Commit)) {
    if ($MO2Version -eq '2.5.2') { $Commit = 'v0.5.6.2' }
    elseif ($MO2Version -eq '2.5.0') { $Commit = 'v0.5.0' }
    else { $Commit = '7368b25' }
}

$ErrorActionPreference = 'Stop'

function Write-Info([string]$Message) {
    Write-Host "[prepare-usvfs] $Message"
}

function Write-Utf8NoBom([string]$Path, [string]$Content) {
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function Wrap-PreprocessorBlock([string]$Text, [string]$Block, [string]$Macro, [string]$NewLine) {
    $guarded = "#ifndef $Macro$NewLine$Block$NewLine#endif  // $Macro"
    if ($Text.Contains($guarded)) {
        return $Text
    }

    if (!$Text.Contains($Block)) {
        Write-Info "Warning: Expected block not found while wrapping with $Macro. Maybe removed in this version."
        return $Text
    }

    return $Text.Replace($Block, $guarded)
}

function Ensure-WholeFileMacroGuard([string]$Path, [string]$Macro) {
    $text = Get-Content -LiteralPath $Path -Raw
    if ($text -match "(?s)^\#ifndef $Macro\b") {
        return $text
    }

    $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $text = "#ifndef $Macro$nl$nl$text$nl#endif  // $Macro$nl"
    Write-Utf8NoBom $Path $text
    return $text
}

function Invoke-GitProcess([string[]]$Arguments, [switch]$Quiet) {
    $stdoutPath = [System.IO.Path]::GetTempFileName()

    try {
        $oldEA = $ErrorActionPreference
        $ErrorActionPreference = 'SilentlyContinue'
        & git @Arguments *> $stdoutPath
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $oldEA

        $stdout = Get-Content -LiteralPath $stdoutPath -Raw

        if (!$Quiet) {
            if ($stdout) {
                Write-Host -NoNewline $stdout
            }
            if ($stdout) {
                Write-Host -NoNewline $stdout
            }
        }

        return @{
            ExitCode = $exitCode
            StdOut = $stdout
        }
    } finally {
        Remove-Item -LiteralPath $stdoutPath -Force -ErrorAction SilentlyContinue
    }
}

function Apply-UsvfsPatchFallback([string]$PatchedSourceDir, [string]$MO2Version) {
    if ($MO2Version -eq '2.4.4') {
        $assemblyMacro = 'USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS;USVFS_TARGET_V244'
    } else {
        $assemblyMacro = 'USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS;USVFS_TARGET_V25X'
        if ($MO2Version -eq '2.5.2') {
            $assemblyMacro += ';USVFS_TARGET_V252'
        }
    }
    $defName = "usvfs_x64_v$($MO2Version.Replace('.','')).def"

    $parametersPath = Join-Path $PatchedSourceDir 'src\usvfs_dll\usvfsparameters.cpp'
    $parametersText = Ensure-WholeFileMacroGuard $parametersPath $assemblyMacro

    $projectPath = Join-Path $PatchedSourceDir 'vsbuild\usvfs_dll.vcxproj'
    Write-Host "[patch-xml] Patching project file $projectPath"
    $xml = [xml](Get-Content -LiteralPath $projectPath)
    $ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $ns.AddNamespace('ms', 'http://schemas.microsoft.com/developer/msbuild/2003')

    # 1. Update PreprocessorDefinitions
    $groups = $xml.SelectNodes("//ms:ItemDefinitionGroup", $ns)
    Write-Host "[patch-xml] Found $($groups.Count) ItemDefinitionGroups"
    foreach ($group in $groups) {
        $condition = $group.Condition
        
        $clCompile = $group.ClCompile
        if ($clCompile) {
            $existing = $clCompile.PreprocessorDefinitions
            if ($existing -and ($existing -notmatch [regex]::Escape($assemblyMacro))) {
                Write-Host "[patch-xml]   Applying preprocessor patch to $condition"
                $clCompile.PreprocessorDefinitions = "BUILDING_USVFS_DLL;BOOST_ALL_NO_LIB;$assemblyMacro;DLLEXPORT=;" + $existing
            }
        }
    }

    # 3. Update ModuleDefinitionFile
    $defX64 = "usvfs_x64_v$MO2Version.def"
    $defX86 = "usvfs_x86_v$MO2Version.def"
    
    $linkGroups = $xml.SelectNodes("//ms:ItemDefinitionGroup/ms:Link", $ns)
    foreach ($link in $linkGroups) {
        $parent = $link.ParentNode
        $condition = $parent.Condition
        $defFile = if ($condition -match 'x64') { "..\..\..\ASSEMBLER\src\$defX64" } else { "..\..\..\ASSEMBLER\src\$defX86" }
        
        if (!$link.ModuleDefinitionFile) {
            $node = $xml.CreateElement('ModuleDefinitionFile', $ns.LookupNamespace('ms'))
            $node.InnerText = $defFile
            $link.AppendChild($node) | Out-Null
        } else {
            $link.ModuleDefinitionFile = $defFile
        }

        # Disable SAFESEH for 32-bit
        if ($condition -notmatch 'x64') {
            if (!$link.ImageHasSafeExceptionHandlers) {
                $node = $xml.CreateElement('ImageHasSafeExceptionHandlers', $ns.LookupNamespace('ms'))
                $link.AppendChild($node) | Out-Null
            }
            $link.ImageHasSafeExceptionHandlers = 'false'
        }
    }

    # 4. Add MASM and Bridge files
    $itemGroups = $xml.SelectNodes("//ms:ItemGroup", $ns)
    
    # Check if files are already added
    $bridgeAdded = $xml.SelectSingleNode("//ms:ClCompile[contains(@Include, 'usvfs_context_bridge.cpp')]", $ns)
    if (!$bridgeAdded) {
        $ig = $xml.CreateElement('ItemGroup', $ns.LookupNamespace('ms'))
        $files = @(
            '..\..\..\ASSEMBLER\src\usvfs_exports_bridge.cpp',
            '..\..\..\ASSEMBLER\src\usvfs_context_bridge.cpp',
            '..\..\..\ASSEMBLER\src\usvfs_kernel32_bridge.cpp',
            '..\..\..\ASSEMBLER\src\usvfs_ntdll_bridge.cpp'
        )
        foreach ($f in $files) {
            $node = $xml.CreateElement('ClCompile', $ns.LookupNamespace('ms'))
            $node.SetAttribute('Include', $f)
            $ig.AppendChild($node) | Out-Null
        }
        $xml.Project.AppendChild($ig) | Out-Null
    }

    $masmAdded = $xml.SelectSingleNode("//ms:MASM[contains(@Include, 'usvfs_context_x86.asm')]", $ns)
    if (!$masmAdded) {
        $ig = $xml.CreateElement('ItemGroup', $ns.LookupNamespace('ms'))
        $asmX86 = @(
            '..\..\..\ASSEMBLER\src\usvfs_parameter_exports_x86.asm',
            '..\..\..\ASSEMBLER\src\usvfs_exports_x86.asm',
            '..\..\..\ASSEMBLER\src\usvfs_runtime_x86.asm',
            '..\..\..\ASSEMBLER\src\usvfs_context_x86.asm'
        )
        foreach ($f in $asmX86) {
            $node = $xml.CreateElement('MASM', $ns.LookupNamespace('ms'))
            $node.SetAttribute('Include', $f)
            $excl = $xml.CreateElement('ExcludedFromBuild', $ns.LookupNamespace('ms'))
            $excl.SetAttribute('Condition', "'`$(Platform)'=='x64'")
            $excl.InnerText = 'true'
            $node.AppendChild($excl) | Out-Null
            $ig.AppendChild($node) | Out-Null
        }
        
        $asmX64 = @(
            '..\..\..\ASSEMBLER\src\usvfs_parameter_exports_x64.asm',
            '..\..\..\ASSEMBLER\src\usvfs_exports_x64.asm',
            '..\..\..\ASSEMBLER\src\usvfs_runtime_x64.asm',
            '..\..\..\ASSEMBLER\src\usvfs_context_x64.asm'
        )
        foreach ($f in $asmX64) {
            $node = $xml.CreateElement('MASM', $ns.LookupNamespace('ms'))
            $node.SetAttribute('Include', $f)
            $excl = $xml.CreateElement('ExcludedFromBuild', $ns.LookupNamespace('ms'))
            $excl.SetAttribute('Condition', "'`$(Platform)'=='Win32'")
            $excl.InnerText = 'true'
            $node.AppendChild($excl) | Out-Null
            $ig.AppendChild($node) | Out-Null
        }
        $xml.Project.AppendChild($ig) | Out-Null
    }

    # 5. Remove original files that are replaced by bridge
    $toRemove = @(
        'hookcallcontext.cpp', 'hookcontext.cpp', 'hookmanager.cpp',
        'kernel32.cpp', 'ntdll.cpp', 'redirectiontree.cpp',
        'semaphore.cpp', 'sharedparameters.cpp', 'usvfs.cpp', 'usvfsparameters.cpp'
    )
    foreach ($name in $toRemove) {
        $nodes = $xml.SelectNodes("//ms:ClCompile[contains(@Include, '$name')]", $ns)
        foreach ($node in $nodes) {
            $node.ParentNode.RemoveChild($node) | Out-Null
        }
    }

    # 6. Enable MASM in project
    if ($xml.SelectSingleNode("//ms:Import[contains(@Project, 'masm.props')]", $ns) -eq $null) {
        $ig = $xml.SelectSingleNode("//ms:ImportGroup[@Label='ExtensionSettings']", $ns)
        if ($ig) {
            $imp = $xml.CreateElement('Import', $ns.LookupNamespace('ms'))
            $imp.SetAttribute('Project', '$(VCTargetsPath)\BuildCustomizations\masm.props')
            $ig.AppendChild($imp) | Out-Null
        }
    }
    if ($xml.SelectSingleNode("//ms:Import[contains(@Project, 'masm.targets')]", $ns) -eq $null) {
        $ig = $xml.SelectSingleNode("//ms:ImportGroup[@Label='ExtensionTargets']", $ns)
        if ($ig) {
            $imp = $xml.CreateElement('Import', $ns.LookupNamespace('ms'))
            $imp.SetAttribute('Project', '$(VCTargetsPath)\BuildCustomizations\masm.targets')
            $ig.AppendChild($imp) | Out-Null
        }
    }

    $xml.Save($projectPath)

    $commonPropsPath = Join-Path $PatchedSourceDir 'vsbuild\usvfs_common.props'
    $commonPropsText = Get-Content -LiteralPath $commonPropsPath -Raw
    if ($commonPropsText -notmatch '\.\.\\src\\usvfs_dll;') {
        $commonPropsText = $commonPropsText.Replace(
            '..\src\shared;..\src\thooklib;..\src\tinjectlib;..\src\usvfs_helper;..\asmjit\src\asmjit;..\udis86;%(AdditionalIncludeDirectories)',
            '..\src\shared;..\src\thooklib;..\src\tinjectlib;..\src\usvfs_helper;..\src\usvfs_dll;..\asmjit\src\asmjit;..\udis86;%(AdditionalIncludeDirectories)')
        Write-Utf8NoBom $commonPropsPath $commonPropsText
    }

    $usvfsPath = Join-Path $PatchedSourceDir 'src\usvfs_dll\usvfs.cpp'
    $usvfsText = Ensure-WholeFileMacroGuard $usvfsPath $assemblyMacro

    $sharedParametersPath = Join-Path $PatchedSourceDir 'src\usvfs_dll\sharedparameters.cpp'
    Ensure-WholeFileMacroGuard $sharedParametersPath $assemblyMacro | Out-Null

    $hookContextPath = Join-Path $PatchedSourceDir 'src\usvfs_dll\hookcontext.cpp'
    Ensure-WholeFileMacroGuard $hookContextPath $assemblyMacro | Out-Null

    $hookManagerPath = Join-Path $PatchedSourceDir 'src\usvfs_dll\hookmanager.cpp'
    Ensure-WholeFileMacroGuard $hookManagerPath $assemblyMacro | Out-Null
    $initLoggingBlock = @"
void WINAPI InitLogging(bool toConsole)
{
  InitLoggingInternal(toConsole, false);
}

extern "C" DLLEXPORT bool WINAPI GetLogMessages(LPSTR buffer, size_t size,
                                                bool blocking)
{
  buffer[0] = '\0';
  try {
    if (blocking) {
      SHMLogger::instance().get(buffer, size);
      return true;
    } else {
      return SHMLogger::instance().tryGet(buffer, size);
    }
  } catch (const std::exception &e) {
    _snprintf_s(buffer, size, _TRUNCATE, "Failed to retrieve log messages: %s",
               e.what());
    return false;
  }
}
"@ -replace "`n", $projectNl
    $updateParamsBlock = @"
void WINAPI USVFSUpdateParams(LogLevel level, CrashDumpsType type)
{
  auto* p = usvfsCreateParameters();

  usvfsSetLogLevel(p, level);
  usvfsSetCrashDumpType(p, type);

  usvfsUpdateParameters(p);
  usvfsFreeParameters(p);
}

void WINAPI usvfsUpdateParameters(usvfsParameters* p)
{
  spdlog::get("usvfs")->info(
    "updating parameters:\n"
    " . debugMode: {}\n"
    " . log level: {}\n"
    " . dump type: {}\n"
    " . dump path: {}\n"
    " . delay process: {}ms",
    p->debugMode, usvfsLogLevelToString(p->logLevel),
    usvfsCrashDumpTypeToString(p->crashDumpsType), p->crashDumpsPath,
    p->delayProcessMs);

  // update actual values used:
  usvfs_dump_type = p->crashDumpsType;
  usvfs_dump_path = ush::string_cast<std::wstring>(
    p->crashDumpsPath, ush::CodePage::UTF8);
  SetLogLevel(p->logLevel);

  // update parameters in context so spawned process will inherit changes:
  context->setDebugParameters(
    p->logLevel, p->crashDumpsType, p->crashDumpsPath,
    std::chrono::milliseconds(p->delayProcessMs));
}
"@ -replace "`n", $projectNl
    $createVfsBlock = @"
void WINAPI GetCurrentVFSName(char *buffer, size_t size)
{
  ush::strncpy_sz(buffer, context->callParameters().currentSHMName, size);
}


// deprecated
//
BOOL WINAPI CreateVFS(const USVFSParameters *oldParams)
{
  const usvfsParameters p(*oldParams);
  const auto r = usvfsCreateVFS(&p);

  return r;
}

BOOL WINAPI usvfsCreateVFS(const usvfsParameters* p)
{
  usvfs::HookContext::remove(p->instanceName);
  return usvfsConnectVFS(p);
}
"@ -replace "`n", $projectNl
    $connectVfsBlock = @"
BOOL WINAPI ConnectVFS(const USVFSParameters *oldParams)
{
  const usvfsParameters p(*oldParams);
  const auto r = usvfsConnectVFS(&p);

  return r;
}

BOOL WINAPI usvfsConnectVFS(const usvfsParameters* params)
{
  if (spdlog::get("usvfs").get() == nullptr) {
    // create temporary logger so we don't get null-pointer exceptions
    spdlog::create<spdlog::sinks::null_sink>("usvfs");
  }

  try {
    DisconnectVFS();
    context = new usvfs::HookContext(*params, dllModule);

    return TRUE;
  } catch (const std::exception &e) {
    spdlog::get("usvfs")->debug("failed to connect to vfs: {}", e.what());
    return FALSE;
  }
}
"@ -replace "`n", $projectNl
    $disconnectVfsBlock = @"
void WINAPI DisconnectVFS()
{
  if (spdlog::get("usvfs").get() == nullptr) {
    // create temporary logger so we don't get null-pointer exceptions
    spdlog::create<spdlog::sinks::null_sink>("usvfs");
  }

  spdlog::get("usvfs")->debug("remove from process {}", GetCurrentProcessId());

  if (manager != nullptr) {
    delete manager;
    manager = nullptr;
  }

  if (context != nullptr) {
    delete context;
    context = nullptr;
    spdlog::get("usvfs")->debug("vfs unloaded");
  }
}
"@ -replace "`n", $projectNl
    $clearMappingsBlock = @"
void WINAPI ClearVirtualMappings()
{
  context->redirectionTable()->clear();
  context->inverseTable()->clear();
}
"@ -replace "`n", $projectNl
    $processListBlock = @"
BOOL WINAPI GetVFSProcessList(size_t *count, LPDWORD processIDs)
{
  if (count == nullptr) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  if (context == nullptr) {
    *count = 0;
  } else {
    std::vector<DWORD> pids = context->registeredProcesses();
    size_t realCount = 0;
    for (DWORD pid : pids) {
      if (processStillActive(pid)) {
        if ((realCount < *count) && (processIDs != nullptr)) {
          processIDs[realCount] = pid;
        }

        ++realCount;
      } // else the process has already ended
    }
    *count = realCount;
  }
  return TRUE;
}

BOOL WINAPI GetVFSProcessList2(size_t* count, DWORD** buffer)
{
  if (!count || !buffer) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  *count = 0;
  *buffer = nullptr;

  std::vector<DWORD> pids = context->registeredProcesses();
  auto last = std::remove_if(pids.begin(), pids.end(), [](DWORD id) {
    return !processStillActive(id);
  });

  pids.erase(last, pids.end());

  if (pids.empty()) {
    return TRUE;
  }

  *count = pids.size();
  *buffer = static_cast<DWORD*>(std::calloc(pids.size(), sizeof(DWORD)));

  std::copy(pids.begin(), pids.end(), *buffer);

  return TRUE;
}
"@ -replace "`n", $projectNl
    $tailExportsBlock = @"
BOOL WINAPI CreateVFSDump(LPSTR buffer, size_t *size)
{
  assert(size != nullptr);
  std::ostringstream output;
  usvfs::shared::dumpTree(output, *context->redirectionTable().get());
  std::string str = output.str();
  if ((buffer != NULL) && (*size > 0)) {
    strncpy_s(buffer, *size, str.c_str(), _TRUNCATE);
  }
  bool success = *size >= str.length();
  *size = str.length();
  return success ? TRUE : FALSE;
}


VOID WINAPI BlacklistExecutable(LPWSTR executableName)
{
  context->blacklistExecutable(executableName);
}


VOID WINAPI ClearExecutableBlacklist()
{
  context->clearExecutableBlacklist();
}


VOID WINAPI ForceLoadLibrary(LPWSTR processName, LPWSTR libraryPath)
{
  context->forceLoadLibrary(processName, libraryPath);
}


VOID WINAPI ClearLibraryForceLoads()
{
  context->clearLibraryForceLoads();
}


VOID WINAPI PrintDebugInfo()
{
  spdlog::get("usvfs")
      ->warn("===== debug {} =====", context->redirectionTable().shmName());
  void *buffer = nullptr;
  size_t bufferSize = 0;
  context->redirectionTable().getBuffer(buffer, bufferSize);
  std::ostringstream temp;
  for (size_t i = 0; i < bufferSize; ++i) {
    temp << std::hex << std::setfill('0') << std::setw(2) << (unsigned)reinterpret_cast<char*>(buffer)[i] << " ";
    if ((i % 16) == 15) {
      spdlog::get("usvfs")->info("{}", temp.str());
      temp.str("");
      temp.clear();
    }
  }
  if (!temp.str().empty()) {
    spdlog::get("usvfs")->info("{}", temp.str());
  }
  spdlog::get("usvfs")
      ->warn("===== / debug {} =====", context->redirectionTable().shmName());
}


// deprecated
//
void WINAPI USVFSInitParameters(USVFSParameters *parameters,
                                const char *instanceName, bool debugMode,
                                LogLevel logLevel,
                                CrashDumpsType crashDumpsType,
                                const char *crashDumpsPath)
{
  parameters->debugMode = debugMode;
  parameters->logLevel = logLevel;
  parameters->crashDumpsType = crashDumpsType;

  strncpy_s(parameters->instanceName, instanceName, _TRUNCATE);
  if (crashDumpsPath && *crashDumpsPath && strlen(crashDumpsPath) < _countof(parameters->crashDumpsPath)) {
    memcpy(parameters->crashDumpsPath, crashDumpsPath, strlen(crashDumpsPath)+1);
    parameters->crashDumpsType = crashDumpsType;
  }
  else {
    // crashDumpsPath invalid or overflow of USVFSParameters variable so disable crash dumps:
    parameters->crashDumpsPath[0] = 0;
    parameters->crashDumpsType = CrashDumpsType::None;
  }
  // we can't use the whole buffer as we need a few bytes to store a running
  // counter
  strncpy_s(parameters->currentSHMName, 60, instanceName, _TRUNCATE);
  memset(parameters->currentInverseSHMName, '\0', _countof(parameters->currentInverseSHMName));
  _snprintf(parameters->currentInverseSHMName, 60, "inv_%s", instanceName);
}


const char* WINAPI USVFSVersionString()
{
  return USVFS_VERSION_STRING;
}
"@ -replace "`n", $projectNl

    $usvfsText = Wrap-PreprocessorBlock $usvfsText $initLoggingBlock $assemblyMacro $projectNl
    $usvfsText = Wrap-PreprocessorBlock $usvfsText $updateParamsBlock $assemblyMacro $projectNl
    $usvfsText = Wrap-PreprocessorBlock $usvfsText $createVfsBlock $assemblyMacro $projectNl
    $usvfsText = Wrap-PreprocessorBlock $usvfsText $connectVfsBlock $assemblyMacro $projectNl
    $usvfsText = Wrap-PreprocessorBlock $usvfsText $disconnectVfsBlock $assemblyMacro $projectNl
    $usvfsText = Wrap-PreprocessorBlock $usvfsText $processListBlock $assemblyMacro $projectNl
    $usvfsText = Wrap-PreprocessorBlock $usvfsText $clearMappingsBlock $assemblyMacro $projectNl
    $usvfsText = Wrap-PreprocessorBlock $usvfsText $tailExportsBlock $assemblyMacro $projectNl
    Write-Utf8NoBom $usvfsPath $usvfsText

    $loggerPath = Join-Path $PatchedSourceDir 'src\shared\shmlogger.cpp'
    $loggerText = Get-Content -LiteralPath $loggerPath -Raw
    $loggerNl = if ($loggerText.Contains("`r`n")) { "`r`n" } else { "`n" }

    if ($loggerText -notmatch '<boost/date_time/posix_time/posix_time\.hpp>') {
        $loggerText = $loggerText.Replace(
            '#include "shmlogger.h"' + $loggerNl,
            '#include "shmlogger.h"' + $loggerNl + '#include <boost/date_time/posix_time/posix_time.hpp>' + $loggerNl)
    }

    if ($loggerText -match 'microsec_clock::universal_time\(\)' -and
        $loggerText -notmatch 'boost::posix_time::microsec_clock::universal_time\(\)') {
        $loggerText = $loggerText.Replace(
            'microsec_clock::universal_time()',
            'boost::posix_time::microsec_clock::universal_time()')
    }

    Write-Utf8NoBom $loggerPath $loggerText
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([System.IO.Path]::IsPathRooted($SourceDir)) {
    $sourceDir = [System.IO.Path]::GetFullPath($SourceDir)
} else {
    $sourceDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $SourceDir))
}
$patchPath = Join-Path $PSScriptRoot 'patches\usvfs-0.5.6.0-asm-parameter-exports.patch'
$boostLinkLibraries = @()

if (!(Test-Path $sourceDir)) {
    $parent = Split-Path -Parent $sourceDir
    if (!(Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }

    Write-Info "Cloning usvfs into $sourceDir"
    Invoke-GitProcess @('clone', $RepoUrl, $sourceDir) | Out-Null
}

Write-Info "Checking out commit $Commit"
Invoke-GitProcess @('-C', $sourceDir, 'fetch', '--all', '--tags') | Out-Null
Invoke-GitProcess @('-C', $sourceDir, 'checkout', '--force', $Commit) | Out-Null
Invoke-GitProcess @('-C', $sourceDir, 'submodule', 'update', '--init', '--recursive') | Out-Null

if ($MO2Version -eq '2.5.2') {
    Write-Info "Copying legacy submodules for v2.5.2 vsbuild compatibility..."
    $v250Path = Join-Path $PSScriptRoot "..\external\usvfs-mo2-v2.5.0"
    if (Test-Path "$v250Path\asmjit") {
        if (Test-Path "$sourceDir\asmjit") { Remove-Item "$sourceDir\asmjit" -Recurse -Force }
        Copy-Item "$v250Path\asmjit" -Destination "$sourceDir\asmjit" -Recurse -Force
    }
    if (Test-Path "$v250Path\spdlog") {
        if (Test-Path "$sourceDir\spdlog") { Remove-Item "$sourceDir\spdlog" -Recurse -Force }
        Copy-Item "$v250Path\spdlog" -Destination "$sourceDir\spdlog" -Recurse -Force
    }
    if (Test-Path "$v250Path\udis86") {
        if (Test-Path "$sourceDir\udis86") { Remove-Item "$sourceDir\udis86" -Recurse -Force }
        Copy-Item "$v250Path\udis86" -Destination "$sourceDir\udis86" -Recurse -Force
    }
}

Write-Info "Applying assembler patch"
$applyCheck = Invoke-GitProcess @('-C', $sourceDir, 'apply', '--check', $patchPath) -Quiet
if ($applyCheck.ExitCode -eq 0) {
    $applyResult = Invoke-GitProcess @('-C', $sourceDir, 'apply', $patchPath)
    if ($applyResult.ExitCode -ne 0) {
        throw "Failed to apply $patchPath"
    }
} else {
    $reverseCheck = Invoke-GitProcess @('-C', $sourceDir, 'apply', '--reverse', '--check', $patchPath) -Quiet
    if ($reverseCheck.ExitCode -eq 0) {
        Write-Info "Patch already present; continuing"
    } else {
        Write-Info "Git patch did not apply cleanly; using scripted fallback"
        Apply-UsvfsPatchFallback $sourceDir $MO2Version
    }
}

$vcxPath = Join-Path $sourceDir "vsbuild\usvfs_dll.vcxproj"
if (Test-Path $vcxPath) {
    $vcxText = Get-Content -LiteralPath $vcxPath -Raw
    if ($MO2Version -eq '2.4.4') {
        $aMacro = 'USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS;USVFS_TARGET_V244'
    } else {
        $aMacro = 'USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS;USVFS_TARGET_V25X'
        if ($MO2Version -eq '2.5.2') { $aMacro += ';USVFS_TARGET_V252' }
    }
    
    if ($vcxText -match 'USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS' -and $vcxText -notmatch 'USVFS_TARGET_V244' -and $vcxText -notmatch 'USVFS_TARGET_V25X') {
        $vcxText = [regex]::Replace($vcxText, '(?i)(<PreprocessorDefinitions>.*?)USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS', '${1}' + $aMacro)
        Write-Utf8NoBom $vcxPath $vcxText
    }
}

$parametersText = Get-Content -LiteralPath (Join-Path $sourceDir 'src\usvfs_dll\usvfsparameters.cpp') -Raw
$sharedParametersText = Get-Content -LiteralPath (Join-Path $sourceDir 'src\usvfs_dll\sharedparameters.cpp') -Raw
$hookContextText = Get-Content -LiteralPath (Join-Path $sourceDir 'src\usvfs_dll\hookcontext.cpp') -Raw
$hookManagerText = Get-Content -LiteralPath (Join-Path $sourceDir 'src\usvfs_dll\hookmanager.cpp') -Raw
$usvfsText = Get-Content -LiteralPath (Join-Path $sourceDir 'src\usvfs_dll\usvfs.cpp') -Raw
$projectText = Get-Content -LiteralPath (Join-Path $sourceDir 'vsbuild\usvfs_dll.vcxproj') -Raw
$commonPropsText = Get-Content -LiteralPath (Join-Path $sourceDir 'vsbuild\usvfs_common.props') -Raw
$loggerText = Get-Content -LiteralPath (Join-Path $sourceDir 'src\shared\shmlogger.cpp') -Raw
if (($parametersText -notmatch '#ifndef USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS') -or
    ($sharedParametersText -notmatch '#ifndef USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS') -or
    ($hookContextText -notmatch '#ifndef USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS') -or
    ($hookManagerText -notmatch '#ifndef USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS') -or
    ($usvfsText -notmatch '#ifndef USVFS_USE_ASSEMBLY_PARAMETER_EXPORTS') -or
    ($projectText -notmatch 'usvfs_parameter_exports_x64\.asm') -or
    ($projectText -notmatch 'usvfs_exports_x64\.asm') -or
    ($projectText -notmatch 'usvfs_runtime_x64\.asm') -or
    ($projectText -notmatch 'usvfs_context_x64\.asm') -or
    ($projectText -notmatch 'usvfs_exports_bridge\.cpp') -or
    ($projectText -notmatch 'usvfs_context_bridge\.cpp') -or
    ($projectText -notmatch 'usvfs_kernel32_bridge\.cpp') -or
    ($projectText -notmatch 'usvfs_ntdll_bridge\.cpp') -or
    ($projectText -match 'usvfs_parameter_exports_bridge\.cpp') -or
    ($projectText -match '\.\.\\src\\usvfs_dll\\hookcallcontext\.cpp') -or
    ($projectText -match '\.\.\\src\\usvfs_dll\\hookcontext\.cpp') -or
    ($projectText -match '\.\.\\src\\usvfs_dll\\hookmanager\.cpp') -or
    ($projectText -match '\.\.\\src\\usvfs_dll\\hooks\\kernel32\.cpp') -or
    ($projectText -match '\.\.\\src\\usvfs_dll\\hooks\\ntdll\.cpp') -or
    ($projectText -match '\.\.\\src\\usvfs_dll\\redirectiontree\.cpp') -or
    ($projectText -match '\.\.\\src\\usvfs_dll\\semaphore\.cpp') -or
    ($projectText -match '\.\.\\src\\usvfs_dll\\sharedparameters\.cpp') -or
    ($projectText -match '\.\.\\src\\usvfs_dll\\usvfs\.cpp') -or
    ($MO2Version -eq '2.4.4') -or
    ([regex]::Matches($projectText, 'USVFS_TARGET_V2').Count -lt 4) -or
    ($projectText -notmatch 'BuildCustomizations\\masm.props') -or
    ($commonPropsText -notmatch '\.\.\\src\\usvfs_dll;') -or
    ($loggerText -notmatch '<boost/date_time/posix_time/posix_time\.hpp>') -or
    ($loggerText -notmatch 'boost::posix_time::microsec_clock::universal_time\(\)')) {
    Write-Info "Patch markers missing or incomplete after git apply; enforcing scripted fallback"
    Apply-UsvfsPatchFallback $sourceDir $MO2Version
}

if (!$BoostPath -and $UseVcpkgBoost) {
    $vcpkgInclude = Join-Path $VcpkgRoot "installed\$Triplet\include\boost"
    $vcpkgLib = Join-Path $VcpkgRoot "installed\$Triplet\lib"

    if (!(Test-Path $vcpkgInclude) -or !(Test-Path $vcpkgLib)) {
        throw "Vcpkg Boost was not found under $VcpkgRoot for triplet $Triplet"
    }

    $boostLinkLibraries = Get-ChildItem -LiteralPath $vcpkgLib -Filter 'boost_*.lib' |
        Sort-Object Name |
        Select-Object -ExpandProperty Name

    $compatRoot = Join-Path $PSScriptRoot "boost-compat-$MO2Version-$Triplet"
    $isX64 = ($Triplet -match 'x64')
    $libSuffix = if ($isX64) { "64" } else { "32" }
    $msvcVersion = if ($MO2Version -eq '2.4.4') { '14.2' } else { '14.3' }
    $compatLibRootName = "lib$($libSuffix)-msvc-$msvcVersion"
    $compatLibRoot = Join-Path $compatRoot $compatLibRootName
    $compatLib = Join-Path $compatLibRoot 'lib'

    if (Test-Path $compatRoot) {
        Remove-Item -LiteralPath $compatRoot -Recurse -Force
    }

    New-Item -ItemType Directory -Path $compatLibRoot | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $compatRoot 'boost') -Target $vcpkgInclude | Out-Null
    New-Item -ItemType Junction -Path $compatLib -Target $vcpkgLib | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $compatRoot 'lib') -Target $vcpkgLib | Out-Null

    $BoostPath = $compatRoot
}

if ($BoostPath) {
    $BoostPath = (Resolve-Path -LiteralPath $BoostPath).Path

    if ($boostLinkLibraries.Count -eq 0) {
        $candidateLibDirs = @(
            (Join-Path $BoostPath "$compatLibRootName\lib"),
            (Join-Path $BoostPath 'lib')
        ) | Where-Object { Test-Path $_ }

        foreach ($libDir in $candidateLibDirs) {
            $boostLinkLibraries = Get-ChildItem -LiteralPath $libDir -Filter 'boost_*.lib' -ErrorAction SilentlyContinue |
                Sort-Object Name |
                Select-Object -ExpandProperty Name
            if ($boostLinkLibraries.Count -gt 0) {
                break
            }
        }
    }

    $propsPath = Join-Path $sourceDir 'vsbuild\external_dependencies_local.props'
    $escapedBoostPath = $BoostPath.Replace('&', '&amp;')
    $boostLinkXml = ''

    if ($boostLinkLibraries.Count -gt 0) {
        $escapedBoostLibraries = (($boostLinkLibraries -join ';') + ';%(AdditionalDependencies)').Replace('&', '&amp;')
        $boostLinkXml = @"
  <ItemDefinitionGroup>
    <Link>
      <AdditionalDependencies>$escapedBoostLibraries</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
"@
    }

    Write-Info "Writing $propsPath"
    @"
<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="UserMacros">
    <BOOST_PATH>$escapedBoostPath</BOOST_PATH>
    <STAGING_PATH_32>..\..\..\install\v$MO2Version\x86</STAGING_PATH_32>
    <STAGING_PATH_64>..\..\..\install\v$MO2Version\x64</STAGING_PATH_64>
    <STAGING_DLL_32>`$(STAGING_PATH_32)\bin</STAGING_DLL_32>
    <STAGING_LIB_32>`$(STAGING_PATH_32)\libs</STAGING_LIB_32>
    <STAGING_PDB_32>`$(STAGING_PATH_32)\pdb</STAGING_PDB_32>
    <STAGING_DLL_64>`$(STAGING_PATH_64)\bin</STAGING_DLL_64>
    <STAGING_LIB_64>`$(STAGING_PATH_64)\libs</STAGING_LIB_64>
    <STAGING_PDB_64>`$(STAGING_PATH_64)\pdb</STAGING_PDB_64>
  </PropertyGroup>
$boostLinkXml
</Project>
"@ | Set-Content -LiteralPath $propsPath -Encoding ASCII
}

$projectPathObj = Join-Path $sourceDir 'vsbuild\usvfs_dll.vcxproj'
if (Test-Path $projectPathObj) {
    $projectTextObj = Get-Content -LiteralPath $projectPathObj -Raw
    $defNameObj = "usvfs_x64_v$($MO2Version.Replace('.','')).def"
    $projectTextObj = [regex]::Replace($projectTextObj, 'usvfs_x64(?:_v\d+)?\.def', $defNameObj)
    Write-Utf8NoBom $projectPathObj $projectTextObj
}

Get-ChildItem -Path $sourceDir -Include '*.vcxproj', '*.props' -Recurse | ForEach-Object {
    $vcxTextOrig = Get-Content -LiteralPath $_.FullName -Raw
    $vcxText = $vcxTextOrig

    if ($vcxText -match '/external:I\$\(BOOST_PATH\)') {
        $vcxText = [regex]::Replace($vcxText, '/external:I\$\(BOOST_PATH\)', '/external:I"$(BOOST_PATH)"')
    }

    if ($MO2Version -eq '2.5.2') {
        if ($vcxText -match 'ud_itab\.py') {
            $vcxText = [regex]::Replace($vcxText, '(?s)<CustomBuildStep>[^<]*<Command>[^<]*python \.\.\\udis86\\scripts\\ud_itab\.py.*?</Command>[^<]*</CustomBuildStep>', '')
        }
        if ($vcxText -match 'test_helpers\.cpp') {
            $vcxText = [regex]::Replace($vcxText, '(?s)<ClCompile Include="\.\.\\src\\shared\\test_helpers\.cpp".*?(?:/>|</ClCompile>)', '')
        }
        if ($vcxText -match '<AdditionalIncludeDirectories>' -and $vcxText -notmatch '\.\.\\include\\usvfs') {
            $vcxText = [regex]::Replace($vcxText, '(?i)(<AdditionalIncludeDirectories>)', '${1}..\include\usvfs;')
        }
        if ($vcxText -match '\\asmjit\\src\\asmjit;') {
            $vcxText = [regex]::Replace($vcxText, '\\asmjit\\src\\asmjit;', '\asmjit\src;')
        }
        if ($vcxText -match '\\udis86;(%\(AdditionalIncludeDirectories\)|<)') {
            $vcxText = [regex]::Replace($vcxText, '\\udis86;(%\(AdditionalIncludeDirectories\)|<)', '\udis86;..\udis86\libudis86;$1')
        }
    }

    if ($vcxText -ne $vcxTextOrig) {
        Write-Utf8NoBom $_.FullName $vcxText
    }
}

Write-Info "Prepared source tree at $sourceDir"
