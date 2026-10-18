param(
    [string]$GamePath,
    [string]$OutputPath = (Join-Path $PSScriptRoot "..\GameAssetsExtracted\ZeroHour"),
    [switch]$All
)

$ErrorActionPreference = 'Stop'

function Read-U32BE([System.IO.BinaryReader]$br) {
    $b = $br.ReadBytes(4)
    if ($b.Length -ne 4) { throw 'Unexpected end of BIG archive.' }
    return [uint32](($b[0] -shl 24) -bor ($b[1] -shl 16) -bor ($b[2] -shl 8) -bor $b[3])
}

function Read-CString([System.IO.BinaryReader]$br) {
    $bytes = New-Object System.Collections.Generic.List[byte]
    while ($true) {
        $v = $br.ReadByte()
        if ($v -eq 0) { break }
        $bytes.Add($v)
        if ($bytes.Count -gt 4096) { throw 'Invalid BIG entry name.' }
    }
    return [Text.Encoding]::ASCII.GetString($bytes.ToArray())
}

function Get-InstallCandidates {
    $c = New-Object System.Collections.Generic.List[string]
    if ($GamePath) { $c.Add($GamePath) }

    $repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    $c.Add((Join-Path $repo 'GeneralsMD\Run'))
    $c.Add($repo)

    $regKeys = @(
        'HKLM:\SOFTWARE\WOW6432Node\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour',
        'HKLM:\SOFTWARE\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour',
        'HKCU:\SOFTWARE\Electronic Arts\EA Games\Command and Conquer Generals Zero Hour'
    )
    foreach ($k in $regKeys) {
        if (Test-Path $k) {
            $p = Get-ItemProperty $k -ErrorAction SilentlyContinue
            foreach ($n in 'InstallPath','Install Dir','Path') {
                if ($p.PSObject.Properties.Name -contains $n) {
                    $v = $p.$n
                    if ($v) { $c.Add([string]$v) }
                }
            }
        }
    }

    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, ${env:ProgramW6432})) {
        if (-not $base) { continue }
        foreach ($rel in @(
            'EA Games\Command and Conquer Generals Zero Hour',
            'EA Games\Command & Conquer Generals Zero Hour',
            'Steam\steamapps\common\Command & Conquer Generals - Zero Hour',
            'Steam\steamapps\common\Command and Conquer Generals - Zero Hour',
            'EA Games\Command and Conquer The Ultimate Collection\Command and Conquer Generals Zero Hour'
        )) { $c.Add((Join-Path $base $rel)) }
    }

    $c | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique
}

function Get-SafeRelativePath([string]$name) {
    $name = $name.Replace('/', '\').TrimStart('\')
    if ([IO.Path]::IsPathRooted($name)) { throw "Rooted path in archive: $name" }
    $parts = $name.Split('\') | Where-Object { $_ -ne '' -and $_ -ne '.' }
    if ($parts -contains '..') { throw "Unsafe path in archive: $name" }
    return ($parts -join [IO.Path]::DirectorySeparatorChar)
}

function Should-Extract([string]$name) {
    if ($All) { return $true }
    $ext = [IO.Path]::GetExtension($name).ToLowerInvariant()
    return $ext -in @('.w3d','.tga','.dds','.bmp','.jpg','.jpeg','.png','.ini','.wnd','.map','.str','.wav','.mp3','.bik','.skn','.fx','.txt')
}

function Extract-Big([string]$archive, [string]$rootOut, [System.Collections.Generic.List[object]]$manifest) {
    Write-Host "[BIG] $archive"
    $fs = [IO.File]::Open($archive,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        $br = New-Object IO.BinaryReader($fs)
        $magic = [Text.Encoding]::ASCII.GetString($br.ReadBytes(4))
        if ($magic -ne 'BIGF' -and $magic -ne 'BIG4') { throw "Unsupported archive signature: $magic" }
        $archiveSize = $br.ReadUInt32() # Generals BIG stores this field little-endian.
        $count = Read-U32BE $br
        $headerEnd = Read-U32BE $br
        if ($count -gt 1000000) { throw "Invalid BIG entry count: $count" }

        $entries = New-Object System.Collections.Generic.List[object]
        for ($i=0; $i -lt $count; $i++) {
            $off = Read-U32BE $br
            $size = Read-U32BE $br
            $name = Read-CString $br
            $entries.Add([pscustomobject]@{ Offset=[uint64]$off; Size=[uint64]$size; Name=$name })
        }

        $archiveName = [IO.Path]::GetFileNameWithoutExtension($archive)
        $archiveOut = Join-Path $rootOut ('archives\' + $archiveName)
        New-Item -ItemType Directory -Force -Path $archiveOut | Out-Null

        $done = 0
        foreach ($e in $entries) {
            if (-not (Should-Extract $e.Name)) { continue }
            if (($e.Offset + $e.Size) -gt $fs.Length) {
                Write-Warning "Skipping out-of-range entry $($e.Name)"
                continue
            }
            $rel = Get-SafeRelativePath $e.Name
            $dest = Join-Path $archiveOut $rel
            $dir = Split-Path -Parent $dest
            if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
            $fs.Position = [int64]$e.Offset
            $remaining = [int64]$e.Size
            $out = [IO.File]::Open($dest,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::None)
            try {
                $buffer = New-Object byte[] 1048576
                while ($remaining -gt 0) {
                    $want = [int][Math]::Min($buffer.Length,$remaining)
                    $read = $fs.Read($buffer,0,$want)
                    if ($read -le 0) { throw "Unexpected EOF extracting $($e.Name)" }
                    $out.Write($buffer,0,$read)
                    $remaining -= $read
                }
            } finally { $out.Dispose() }
            $manifest.Add([pscustomobject]@{
                Archive=[IO.Path]::GetFileName($archive)
                Asset=$e.Name
                Extension=[IO.Path]::GetExtension($e.Name).ToLowerInvariant()
                Size=[uint64]$e.Size
                ExtractedPath=$dest
            })
            $done++
        }
        Write-Host "      extracted $done / $count entries"
    } finally { $fs.Dispose() }
}

$candidates = @(Get-InstallCandidates)
$gameRoot = $null
foreach ($c in $candidates) {
    $bigs = @(Get-ChildItem -LiteralPath $c -Filter *.big -File -ErrorAction SilentlyContinue)
    if ($bigs.Count -gt 0) { $gameRoot = (Resolve-Path $c).Path; break }
}
if (-not $gameRoot) {
    Write-Host 'No Zero Hour .BIG archives were found automatically.' -ForegroundColor Yellow
    Write-Host 'Run again with: .\extract_game_assets.cmd "C:\path\to\Zero Hour"'
    exit 2
}

Write-Host "Zero Hour root: $gameRoot" -ForegroundColor Cyan
$archives = @(Get-ChildItem -LiteralPath $gameRoot -Filter *.big -File | Sort-Object Name)
if ($archives.Count -eq 0) { throw "No .BIG files found in $gameRoot" }

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
$manifest = New-Object System.Collections.Generic.List[object]
foreach ($a in $archives) { Extract-Big $a.FullName $OutputPath $manifest }

$manifestCsv = Join-Path $OutputPath 'asset_manifest.csv'
$manifest | Export-Csv -NoTypeInformation -Encoding UTF8 $manifestCsv
$manifestJson = Join-Path $OutputPath 'asset_manifest.json'
$manifest | ConvertTo-Json -Depth 3 | Set-Content -Encoding UTF8 $manifestJson

$summary = $manifest | Group-Object Extension | Sort-Object Count -Descending | ForEach-Object {
    [pscustomobject]@{ Extension=$_.Name; Count=$_.Count; Bytes=($_.Group | Measure-Object Size -Sum).Sum }
}
$summary | Format-Table -AutoSize
Write-Host "Extracted assets: $($manifest.Count)" -ForegroundColor Green
Write-Host "Output: $OutputPath"
Write-Host "Manifest: $manifestCsv"
