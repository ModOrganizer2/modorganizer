[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

function Write-Utf8NoBom([string]$Path, [string]$Content) {
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function Split-CodeAndComment([string]$Line) {
    $inString = $false
    for ($i = 0; $i -lt $Line.Length; $i++) {
        $ch = $Line[$i]
        if ($ch -eq "'") {
            if ($inString -and ($i + 1) -lt $Line.Length -and $Line[$i + 1] -eq "'") {
                $i++
                continue
            }

            $inString = -not $inString
            continue
        }

        if (-not $inString -and $ch -eq ';') {
            return @($Line.Substring(0, $i), $Line.Substring($i))
        }
    }

    return @($Line, '')
}

$globalAliases = @{}
$globalAliasIndex = 0
$scopedAliases = @{}
$scopedAliasIndex = 0

function Get-GlobalAlias([string]$Token) {
    if (-not $globalAliases.ContainsKey($Token)) {
        $globalAliases[$Token] = '__lsym_{0:X4}' -f $globalAliasIndex
        $script:globalAliasIndex++
    }

    return $globalAliases[$Token]
}

function Get-ScopedAlias([string]$Scope, [string]$Token) {
    $key = "$Scope`n$Token"
    if (-not $scopedAliases.ContainsKey($key)) {
        $scopedAliases[$key] = '__ll_{0:X4}' -f $scopedAliasIndex
        $script:scopedAliasIndex++
    }

    return $scopedAliases[$key]
}

function Get-LineScope([string]$RawCode, [string]$CurrentProc, [string]$CurrentMetadataScope) {
    if ($RawCode -match '^\s*(\S+)\s+PROC\b') {
        return $Matches[1]
    }

    if ($RawCode -match '^\s*\$pdata\$(?:\d+\$)?(\S+)\s+DD\b') {
        return $Matches[1]
    }

    if ($RawCode -match '^\s*\$(?:ip2state|stateUnwindMap|cppxdata|unwind)\$(\S+)\b') {
        return $Matches[1]
    }

    if (-not [string]::IsNullOrWhiteSpace($CurrentMetadataScope)) {
        return $CurrentMetadataScope
    }

    return $CurrentProc
}

function Convert-OutsideStrings([string]$Text, [string]$Scope) {
    $scopeKey = if ([string]::IsNullOrWhiteSpace($Scope)) { '__global__' } else { $Scope }
    $builder = New-Object System.Text.StringBuilder
    $segment = New-Object System.Text.StringBuilder
    $inString = $false

    $flush = {
        if ($segment.Length -le 0) {
            return
        }

        $part = $segment.ToString()

        # Compiler-generated stack/local pseudo names use angle brackets and are not
        # accepted by ml64.
        $part = [regex]::Replace($part, '<[^>\r\n]+>\$?', {
            param($m)
            Get-GlobalAlias $m.Value
        })

        # MSVC emits many short local labels that are only unique inside a single
        # function. We globalize them so pdata/xdata references can resolve.
        $part = [regex]::Replace($part, '(?<![A-Za-z0-9_?$@])(\$LN[0-9A-Za-z_@$?]+)', {
            param($m)
            Get-ScopedAlias $scopeKey $m.Groups[1].Value
        })
        $part = [regex]::Replace($part, '(?<![A-Za-z0-9_?$@])(__try(?:begin|end)\$[^\s:+\],]+)', {
            param($m)
            Get-GlobalAlias $m.Groups[1].Value
        })
        $part = [regex]::Replace($part, '(?<![A-Za-z0-9_?$@])(__catch\$[^\s:+\],]+)', {
            param($m)
            Get-GlobalAlias $m.Groups[1].Value
        })

        # Long mangled names and angle-bracket lambda names exceed ml64 limits.
        $part = [regex]::Replace($part, '(?<![A-Za-z0-9_?$@])[$?@A-Za-z_][$?@A-Za-z0-9_<>@\$]*', {
            param($m)
            $token = $m.Value
            if ($token.Contains('<') -or $token.Contains('>') -or $token.Length -gt 200) {
                return (Get-GlobalAlias $token)
            }

            return $token
        })

        # ml64 accepts OFFSET, not FLAT:, in these contexts.
        $part = $part -replace '\b(DD|DQ)\s+FLAT:(\S+)', '$1 OFFSET $2'
        $part = $part -replace '\bOFFSET\s+FLAT:(\S+)', 'OFFSET $1'
        $part = $part -replace '(?<![A-Za-z0-9_?$@])([A-Fa-f][0-9A-Fa-f]+H)(?![A-Za-z0-9_?$@])', '0$1'

        # Raw CL listing uses gs:88, ml64 wants gs:[88].
        $part = $part -replace '\b(gs|fs):([0-9A-Fa-f]+H?|\d+)', '$1:[$2]'
        $part = $part -replace '\block_bts\b', 'lock bts'
        $part = $part -replace '\brex_jmp\b', 'jmp'
        $part = $part -replace '\bSHORT\s+', ''

        # `lea reg, OFFSET sym` is rejected by ml64; `mov reg, OFFSET sym` is fine
        # and preserves semantics here because flags are irrelevant.
        $part = $part -replace '^(\s*)lea(\s+)([^,]+),(\s+)OFFSET (\S+)\s*$', '$1mov$2$3,$4OFFSET $5'

        # CL emits a width that ml64 rejects for this constant mask operand.
        $part = $part -replace '^(\s*xorps\s+xmm\d+,\s+)QWORD PTR (__xmm@\S+)\s*$', '$1XMMWORD PTR $2'
        $part = $part -replace '^(\s*movzx\s+)al(\s*,\s+r\d+b)\s*$', '${1}eax$2'

        [void]$builder.Append($part)
        $segment.Clear() | Out-Null
    }

    for ($i = 0; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if ($ch -eq "'") {
            if (-not $inString) {
                & $flush
                $inString = $true
                [void]$builder.Append($ch)
                continue
            }

            if (($i + 1) -lt $Text.Length -and $Text[$i + 1] -eq "'") {
                [void]$builder.Append("''")
                $i++
                continue
            }

            $inString = $false
            [void]$builder.Append($ch)
            continue
        }

        if ($inString) {
            [void]$builder.Append($ch)
        } else {
            [void]$segment.Append($ch)
        }
    }

    & $flush
    return $builder.ToString()
}

$inputLines = Get-Content -LiteralPath $InputPath
$outputLines = New-Object System.Collections.Generic.List[string]
$currentProc = $null
$currentMetadataScope = $null
$skipVoltbl = $false

foreach ($line in $inputLines) {
    $split = Split-CodeAndComment $line
    $rawCode = $split[0].TrimEnd()

    if ($skipVoltbl) {
        if ($rawCode -match '^\s*voltbl\s+ENDS\s*$') {
            $skipVoltbl = $false
        }
        continue
    }

    if ($rawCode -match '^\s*voltbl\s+SEGMENT\s*$') {
        $skipVoltbl = $true
        continue
    }

    if ([string]::IsNullOrWhiteSpace($rawCode)) {
        $outputLines.Add('')
        continue
    }

    if ($rawCode -match '^\s*\$pdata\$(?:\d+\$)?(\S+)\s+DD\b') {
        $currentMetadataScope = $Matches[1]
    } elseif ($rawCode -match '^\s*\$(?:ip2state|stateUnwindMap|cppxdata|unwind)\$(\S+)\b') {
        $currentMetadataScope = $Matches[1]
    } elseif ($rawCode -match '^\s*(?:pdata|xdata)\s+ENDS\b') {
        $currentMetadataScope = $null
    }

    $lineScope = Get-LineScope -RawCode $rawCode -CurrentProc $currentProc -CurrentMetadataScope $currentMetadataScope
    $orgMatch = [regex]::Match(
        $rawCode,
        '^(?<indent>\s*)(?:(?<label>\S+)\s+)?ORG\s+\$\+(?<count>[0-9A-Fa-f]+H?|\d+)\s*$')

    if ($orgMatch.Success) {
        $indent = $orgMatch.Groups['indent'].Value
        $label = $orgMatch.Groups['label'].Value
        $countText = $orgMatch.Groups['count'].Value
        $count = if ($countText -match '^[0-9A-Fa-f]+H$') {
            [Convert]::ToInt32($countText.Substring(0, $countText.Length - 1), 16)
        } else {
            [int]$countText
        }

        if ($label) {
            $outputLines.Add((Convert-OutsideStrings -Text "$indent$label LABEL BYTE" -Scope $lineScope))
        }
        $outputLines.Add("${indent}DB $count DUP (0)")
    } else {
        $processed = Convert-OutsideStrings -Text $rawCode -Scope $lineScope
        if ($rawCode -match '^\s*(\$LN[0-9A-Za-z_@$?]+|__try(?:begin|end)\$[^\s:+\],]+|__catch\$[^\s:+\],]+):\s*$') {
            $processed = $processed -replace ':\s*$', '::'
        }
        $outputLines.Add($processed)

        # Some MSVC funclet listings reference a synthetic catch label but never
        # emit the label itself. Anchoring it at fallthrough is enough for ml64.
        if ($rawCode -match '^\s*lea\s+rax,\s+(\$LN\d+@catch\$\d+)\s*$') {
            $scopeKey = if ([string]::IsNullOrWhiteSpace($lineScope)) { '__global__' } else { $lineScope }
            $outputLines.Add((Get-ScopedAlias $scopeKey $Matches[1]) + '::')
        }
    }

    if ($rawCode -match '^\s*(\S+)\s+PROC\b') {
        $currentProc = $Matches[1]
    } elseif ($rawCode -match '^\s*\S+\s+ENDP\b') {
        $currentProc = $null
    }
}

Write-Utf8NoBom -Path $OutputPath -Content (($outputLines -join "`r`n") + "`r`n")

Write-Host ("[convert-cl-listing-to-ml64] globals={0} scoped={1}" -f $globalAliasIndex, $scopedAliasIndex)
