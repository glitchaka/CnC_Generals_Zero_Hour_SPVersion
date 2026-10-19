$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$ext=Join-Path $root 'ExternalAssets'
$dFull=Join-Path $ext 'KayKitDungeonFull'
$aFull=Join-Path $ext 'KayKitAdventurersFull'
if(-not(Test-Path $dFull)){throw 'Falta KayKitDungeonFull'}
if(-not(Test-Path $aFull)){throw 'Falta KayKitAdventurersFull'}
$d=Join-Path $ext 'KayKitDungeon';New-Item -ItemType Directory -Force -Path $d|Out-Null
foreach($n in @('rubble_large.obj','crates_stacked.obj','barrel_large.obj')){Copy-Item -Force (Join-Path $dFull ('Assets\obj\'+$n)) (Join-Path $d $n)}
Copy-Item -Force (Join-Path $dFull 'Assets\textures\dungeon_texture.png') (Join-Path $d 'dungeon_texture.png')
$a=Join-Path $ext 'KayKitAdventurers';New-Item -ItemType Directory -Force -Path $a|Out-Null
foreach($n in @('Knight.glb','Ranger.glb','knight_texture.png','ranger_texture.png')){Copy-Item -Force (Join-Path $aFull ('Characters\gltf\'+$n)) (Join-Path $a $n)}
foreach($n in @('sword_1handed.obj','sword_2handed.obj','shield_round.obj')){Copy-Item -Force (Join-Path $aFull ('Assets\obj\'+$n)) (Join-Path $a $n)}
$anim=Join-Path $aFull 'CharacterAnimations\Animations\gltf\Rig_Medium'
foreach($n in @('Rig_Medium_General.glb','Rig_Medium_MovementBasic.glb','Rig_Medium_MovementAdvanced.glb','Rig_Medium_CombatMelee.glb')){Copy-Item -Force (Join-Path $anim $n) (Join-Path $a $n)}
Write-Host 'Assets runtime derivados de los packs completos.'
