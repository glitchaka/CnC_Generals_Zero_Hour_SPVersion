param(
    [string]$ZipPath,
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\ExternalAssets\KayKitDungeon')
)

$ErrorActionPreference = 'Stop'

function Find-KayKitZip {
    if ($ZipPath -and (Test-Path -LiteralPath $ZipPath)) { return (Resolve-Path -LiteralPath $ZipPath).Path }

    $repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    $candidates = @(
        (Join-Path $repo 'KayKit_Dungeon_Pack_1.1_FREE.zip'),
        (Join-Path $env:USERPROFILE 'Downloads\KayKit_Dungeon_Pack_1.1_FREE.zip'),
        (Join-Path $env:USERPROFILE 'Desktop\KayKit_Dungeon_Pack_1.1_FREE.zip')
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { return (Resolve-Path -LiteralPath $c).Path }
    }

    foreach ($dir in @((Join-Path $env:USERPROFILE 'Downloads'), (Join-Path $env:USERPROFILE 'Desktop'))) {
        if (-not (Test-Path -LiteralPath $dir)) { continue }
        $f = Get-ChildItem -LiteralPath $dir -Filter 'KayKit*Dungeon*FREE*.zip' -File -ErrorAction SilentlyContinue |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($f) { return $f.FullName }
    }
    return $null
}

$zip = Find-KayKitZip
if (-not $zip) {
    Write-Host 'No encontre KayKit_Dungeon_Pack_1.1_FREE.zip.' -ForegroundColor Yellow
    Write-Host 'Pon el ZIP en Descargas, Escritorio o en la raiz del repositorio.'
    exit 2
}

Write-Host "Importando KayKit Dungeon desde: $zip" -ForegroundColor Cyan
$tmp = Join-Path $env:TEMP ('sp-kaykit-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
try {
    Expand-Archive -LiteralPath $zip -DestinationPath $tmp -Force
    $src = Get-ChildItem -LiteralPath $tmp -Directory | Select-Object -First 1
    if (-not $src) { throw 'El ZIP no contiene la carpeta raiz esperada.' }
    $obj = Join-Path $src.FullName 'Assets\obj'
    if (-not (Test-Path -LiteralPath $obj)) { throw 'No encontre Assets\obj dentro del pack.' }

    New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
    $wanted = @(
        'floor_dirt_large.obj',
        'floor_dirt_large_rocky.obj',
        'rubble_large.obj',
        'crates_stacked.obj',
        'barrel_large.obj',
        'dungeon_texture.png'
    )
    foreach ($name in $wanted) {
        $from = Join-Path $obj $name
        if (-not (Test-Path -LiteralPath $from)) { throw "Falta $name en el pack KayKit." }
        Copy-Item -LiteralPath $from -Destination (Join-Path $OutputPath $name) -Force
    }

    @'
KayKit : Dungeon Asset Pack 1.1
Created/distributed by Kay Lousberg
License: Creative Commons Zero (CC0)
https://creativecommons.org/publicdomain/zero/1.0/
https://kaylousberg.itch.io/kaykit-dungeon-pack
'@ | Set-Content -Encoding UTF8 (Join-Path $OutputPath 'LICENSE.txt')

    Write-Host "KayKit listo en: $OutputPath" -ForegroundColor Green
} finally {
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
