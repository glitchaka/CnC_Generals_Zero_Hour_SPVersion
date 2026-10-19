$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'ExternalAssets\KayKitDungeon'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$base = 'https://raw.githubusercontent.com/GeorgeQLe/assets-kaykit-3d-environments/main/assets/kaykit/dungeon-remastered-1.1/Assets/obj'
$files = @(
    'barrel_large.obj',
    'crates_stacked.obj',
    'rubble_large.obj',
    'dungeon_texture.png'
)

foreach ($name in $files) {
    $dst = Join-Path $out $name
    if (-not (Test-Path $dst)) {
        Write-Host "Descargando KayKit Dungeon 1.1: $name"
        Invoke-WebRequest -UseBasicParsing -Uri "$base/$name" -OutFile $dst
    }

    if ((Get-Item $dst).Length -le 0) {
        throw "KayKit Dungeon produjo un archivo vacio: $name"
    }
}

@'
KayKit Dungeon Remastered 1.1
Original creator: Kay Lousberg / KayKit
License: CC0 1.0 Universal
Source mirror used by the deterministic build fetch:
https://github.com/GeorgeQLe/assets-kaykit-3d-environments/tree/main/assets/kaykit/dungeon-remastered-1.1

Only the runtime subset used by SPVersion is downloaded:
- barrel_large.obj
- crates_stacked.obj
- rubble_large.obj
- dungeon_texture.png
'@ | Set-Content -Encoding UTF8 (Join-Path $out 'SOURCE.txt')

Write-Host 'KayKit Dungeon listo.'
