[CmdletBinding()]
param(
    [string]$MO2Version = '2.4.4',
    [string]$SourceDir,
    [switch]$UseVcpkgBoost,
    [string]$VcpkgRoot = 'C:\vcpkg',
    [string]$Triplet = 'x64-windows-static',
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

$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
if (!(Test-Path $msbuild)) {
    $msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe'
}
if (!(Test-Path $msbuild)) {
    throw 'MSBuild.exe was not found'
}

& $msbuild `
    (Join-Path $SourceDir 'vsbuild\usvfs.sln') `
    /m `
    /p:Configuration=$Configuration `
    /p:Platform=x64 `
    /p:PlatformToolset=v143 `
    /nr:false

if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}
