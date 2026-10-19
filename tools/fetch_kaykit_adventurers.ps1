$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'ExternalAssets\KayKitAdventurers'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$base = 'https://raw.githubusercontent.com/GeorgeQLe/assets-kaykit-3d-characters/main/assets/kaykit/adventurers-2.0'
$animBase = 'https://raw.githubusercontent.com/GeorgeQLe/assets-kaykit-3d-characters/main/assets/kaykit/character-animations-1.1/Animations/gltf/Rig_Medium'
$files = @{
  'Knight.glb' = "$base/Characters/gltf/Knight.glb"
  'Ranger.glb' = "$base/Characters/gltf/Ranger.glb"
  'knight_texture.png' = "$base/Characters/gltf/knight_texture.png"
  'ranger_texture.png' = "$base/Characters/gltf/ranger_texture.png"
  'Rig_Medium_General.glb' = "$animBase/Rig_Medium_General.glb"
  'Rig_Medium_MovementBasic.glb' = "$animBase/Rig_Medium_MovementBasic.glb"
  'Rig_Medium_MovementAdvanced.glb' = "$animBase/Rig_Medium_MovementAdvanced.glb"
  'Rig_Medium_CombatMelee.glb' = "$animBase/Rig_Medium_CombatMelee.glb"
  'sword_1handed.obj' = "$base/Assets/obj/sword_1handed.obj"
  'sword_2handed.obj' = "$base/Assets/obj/sword_2handed.obj"
  'shield_round.obj' = "$base/Assets/obj/shield_round.obj"
  'KayKit-CC0-License.txt' = 'https://raw.githubusercontent.com/GeorgeQLe/assets-kaykit-3d-characters/main/LICENSES/KayKit-CC0-License.txt'
}
foreach ($name in $files.Keys) {
  $dst = Join-Path $out $name
  if (-not (Test-Path $dst)) {
    Write-Host "Descargando KayKit Adventurers/Animations: $name"
    Invoke-WebRequest -UseBasicParsing -Uri $files[$name] -OutFile $dst
  }
}
Write-Host 'KayKit Adventurers y animaciones de combate listos.'
