[CmdletBinding()]
param(
    [string]$MO2Version = '2.4.4',
    [string]$SourceDir,
    [switch]$UseVcpkgBoost,
    [string]$VcpkgRoot = 'C:\vcpkg',
    [string]$Triplet = 'x86-windows-static',
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = (Join-Path $PSScriptRoot "..\external\usvfs-mo2-v$MO2Version")
}

$prepareArgs = @{
    MO2Version = $MO2Version
    SourceDir = $SourceDir
}

if ($UseVcpkgBoost) {
    $prepareArgs.UseVcpkgBoost = $true
    $prepareArgs.VcpkgRoot = $VcpkgRoot
    $prepareArgs.Triplet = $Triplet
}

& (Join-Path $PSScriptRoot 'prepare-usvfs-source.ps1') @prepareArgs

$msbuild = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -products * -nologo -latest -version '[17.0,19.0)' -property installationPath
    if ($vsPath) {
        $candidate = Join-Path $vsPath 'MSBuild\Current\Bin\amd64\MSBuild.exe'
        if (Test-Path $candidate) {
            $msbuild = $candidate
        }
    }
}
if (-not $msbuild) {
    $candidate = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
    if (Test-Path $candidate) {
        $msbuild = $candidate
    }
}
if (-not $msbuild) {
    throw 'MSBuild.exe was not found'
}

# In usvfs.sln, the platform is named "x86", but in .vcxproj it is "Win32".
# MSBuild Solution platform property overrides project platform.
& $msbuild `
    (Join-Path $SourceDir 'vsbuild\usvfs.sln') `
    /m `
    /v:n `
    /p:Configuration=$Configuration `
    /p:Platform=x86 `
    /p:PlatformToolset=v143 `
    /p:STAGING_PATH_32="..\..\..\install\v$MO2Version\x86" `
    /p:STAGING_PATH_64="..\..\..\install\v$MO2Version\x64" `
    /nr:false

if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}
