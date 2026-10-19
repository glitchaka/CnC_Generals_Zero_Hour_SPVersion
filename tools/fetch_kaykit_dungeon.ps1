$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$archive = Join-Path $root 'EmbeddedAssets\KayKitDungeon\runtime_subset.zip'
$out = Join-Path $root 'ExternalAssets\KayKitDungeon'

if (-not (Test-Path -LiteralPath $archive)) {
    throw "Falta el paquete KayKit integrado en el repositorio: $archive"
}

New-Item -ItemType Directory -Force -Path $out | Out-Null
Expand-Archive -LiteralPath $archive -DestinationPath $out -Force

$required = @(
    'floor_dirt_large.obj',
    'floor_dirt_large_rocky.obj',
    'rubble_large.obj',
    'crates_stacked.obj',
    'barrel_large.obj',
    'dungeon_texture.png',
    'LICENSE.txt'
)

foreach ($name in $required) {
    $path = Join-Path $out $name
    if (-not (Test-Path -LiteralPath $path)) {
        throw "El paquete KayKit integrado no contiene: $name"
    }
    if ((Get-Item -LiteralPath $path).Length -le 0) {
        throw "El paquete KayKit produjo un archivo vacio: $name"
    }
}

Write-Host 'KayKit Dungeon listo desde el asset integrado en el repositorio.'
