param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente final: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$required=@(
  'buildHexFloor()',
  'g_hexFloorTextured',
  'g_hexFloorTransit',
  'updateMobs(dt)',
  'damageNearestMob()',
  'g_boundaryWallMesh',
  'Rig_Medium_CombatMelee.glb',
  'Melee_1H_Attack_Chop',
  'findHandBone',
  'g_gameState==GameState::Defeated){g_gameState=GameState::Playing',
  'SetProcessDpiAwarenessContext'
)
foreach($token in $required){if(-not $code.Contains($token)){throw "Release incompleto: falta $token"}}
if($code.Contains('buildLaneRibbon(laneTop(),0);')){throw 'Release invalido: aun intenta construir cintas de carril sobre el piso hexagonal.'}
if($code.Contains('if(g_player.health<=0.f)g_gameState=GameState::Defeated;')){throw 'Release invalido: queda una ruta de muerte que abre pantalla de derrota.'}
Write-Host 'Fuente final validado: piso hexagonal completo, NPC, combate animado, muro, DPI y respawn automatico presentes.'
