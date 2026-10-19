param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente final: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw

$required=@(
  'std::array<Mob,12>',
  'updateMobs(dt)',
  'damageNearestMob()',
  'drawMobs()',
  'g_mobsRemaining/12.f',
  'g_waveNumber',
  'resolvePlayerMobCollision()',
  'target>=8.f?ey',
  'buildGroundRibbon(',
  'buildLaneRibbon(laneTop(),0);',
  'g_boundaryWallMesh',
  'Rig_Medium_CombatMelee.glb',
  'Melee_1H_Attack_Chop',
  'forearm',
  'lowerarm',
  'SetProcessDpiAwarenessContext'
)
foreach($token in $required){if(-not $code.Contains($token)){throw "Release incompleto: falta $token"}}

$forbidden=@(
  'buildHexFloor()',
  'g_hexFloorTextured',
  'g_hexFloorTransit',
  'drawCharacter(g_npc);',
  'g_gameState=GameState::Victory',
  'R - REINICIAR',
  '392.f*g_enemyHealth',
  'if(g_player.health<=0.f)g_gameState=GameState::Defeated;'
)
foreach($token in $forbidden){if($code.Contains($token)){throw "Release invalido: todavia contiene $token"}}

Write-Host 'Fuente final validado: 12 enemigos por oleada, oleadas automaticas, suelo continuo, edificios escalados por altura, sin NPC unico ni pantalla de reinicio.'
