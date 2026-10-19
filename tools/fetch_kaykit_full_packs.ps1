$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$ext=Join-Path $root 'ExternalAssets'
New-Item -ItemType Directory -Force -Path $ext | Out-Null
$tmp=Join-Path $env:TEMP ('sp-kaykit-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
try {
  $envRepo=Join-Path $tmp 'env'
  git clone --depth 1 --filter=blob:none --sparse https://github.com/GeorgeQLe/assets-kaykit-3d-environments.git $envRepo | Out-Null
  if($LASTEXITCODE -ne 0){throw 'No pude clonar assets-kaykit-3d-environments'}
  git -C $envRepo sparse-checkout set assets/kaykit/dungeon-remastered-1.1 assets/kaykit/forest-nature-pack-1.0 assets/kaykit/medieval-hexagon-pack-1.0.1
  if($LASTEXITCODE -ne 0){throw 'No pude preparar los packs de escenario'}
  $map=@{
    'dungeon-remastered-1.1'='KayKitDungeonFull'
    'forest-nature-pack-1.0'='KayKitForest'
    'medieval-hexagon-pack-1.0.1'='KayKitMedieval'
  }
  foreach($k in $map.Keys){
    $src=Join-Path $envRepo ('assets\kaykit\'+$k)
    $dst=Join-Path $ext $map[$k]
    if(Test-Path $dst){Remove-Item -Recurse -Force $dst}
    Copy-Item -Recurse -Force $src $dst
  }

  $charRepo=Join-Path $tmp 'chars'
  git clone --depth 1 --filter=blob:none --sparse https://github.com/GeorgeQLe/assets-kaykit-3d-characters.git $charRepo | Out-Null
  if($LASTEXITCODE -ne 0){throw 'No pude clonar assets-kaykit-3d-characters'}
  git -C $charRepo sparse-checkout set assets/kaykit/adventurers-2.0 assets/kaykit/character-animations-1.1
  if($LASTEXITCODE -ne 0){throw 'No pude preparar Adventurers completo'}
  $dst=Join-Path $ext 'KayKitAdventurersFull'
  if(Test-Path $dst){Remove-Item -Recurse -Force $dst}
  New-Item -ItemType Directory -Force -Path $dst | Out-Null
  Copy-Item -Recurse -Force (Join-Path $charRepo 'assets\kaykit\adventurers-2.0\*') $dst
  Copy-Item -Recurse -Force (Join-Path $charRepo 'assets\kaykit\character-animations-1.1') (Join-Path $dst 'CharacterAnimations')
  Write-Host 'Cuatro packs KayKit completos preparados.'
}
finally { if(Test-Path $tmp){Remove-Item -Recurse -Force $tmp} }
