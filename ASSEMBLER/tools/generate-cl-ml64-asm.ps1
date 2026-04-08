[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceFile,

    [Parameter(Mandatory = $true)]
    [string]$OutputAsm,

    [string]$Compiler = 'cl.exe',
    [string[]]$Definitions = @(),
    [string[]]$IncludeDirectories = @(),
    [string[]]$AdditionalCompilerArgs = @()
)

$ErrorActionPreference = 'Stop'

$sourcePath = (Resolve-Path -LiteralPath $SourceFile).Path
$outputPath = [System.IO.Path]::GetFullPath($OutputAsm)
$outputDir = Split-Path -Parent $outputPath
if (-not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$rawAsmPath = [System.IO.Path]::ChangeExtension($outputPath, '.cl.raw.asm')
$nullObjPath = [System.IO.Path]::Combine($outputDir, 'cl-null.obj')
$sanitizerPath = Join-Path $PSScriptRoot 'convert-cl-listing-to-ml64.ps1'

$args = @(
    '/nologo',
    '/c',
    '/FA',
    "/Fa$rawAsmPath",
    "/Fo$nullObjPath"
)

foreach ($definition in $Definitions) {
    $args += "/D$definition"
}

foreach ($includeDirectory in $IncludeDirectories) {
    $args += "/I$includeDirectory"
}

$args += $AdditionalCompilerArgs
$args += $sourcePath

Write-Host "[generate-cl-ml64-asm] $Compiler $($args -join ' ')"
& $Compiler @args
if ($LASTEXITCODE -ne 0) {
    throw "cl.exe failed with exit code $LASTEXITCODE"
}

& $sanitizerPath -InputPath $rawAsmPath -OutputPath $outputPath
if ($LASTEXITCODE -ne 0) {
    throw "sanitize step failed with exit code $LASTEXITCODE"
}
