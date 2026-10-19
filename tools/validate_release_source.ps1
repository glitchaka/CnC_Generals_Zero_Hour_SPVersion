param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente final: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw

$required=@(
  'std::array<Mob,12>',
  'mobLane(int lane)',
  'updateMobs(dt)',
  'damageNearestMob()',
  'drawMobs()',
  'g_mobsRemaining/12.f',
  'g_waveNumber',
  'resolvePlayerMobCollision()',
  'target>=8.f?ey',
  'buildHexFloor()',
  'g_hexFloorTextured',
  'g_hexFloorTransit',
  'hex_grass.obj',
  'blocksGameplay(cx,cz,S*1.42f)',
  'drawMeshWorld(g_hexFloorTextured',
  'drawMeshWorld(g_hexFloorTransit',
  'g_boundaryWallMesh',
  'const int N=52',
  'Rig_Medium_CombatMelee.glb',
  'Melee_1H_Attack_Chop',
  'forearm',
  'lowerarm',
  'SetProcessDpiAwarenessContext'
)
foreach($token in $required){if(-not $code.Contains($token)){throw "Release incompleto: falta $token"}}

$forbidden=@(
  'buildLaneRibbon(laneTop(),0);',
  'buildLaneRibbon(laneMid(),1);',
  'buildLaneRibbon(laneBot(),2);',
  'drawCharacter(g_npc);',
  'g_gameState=GameState::Victory',
  'R - REINICIAR',
  '392.f*g_enemyHealth',
  'if(g_player.health<=0.f)g_gameState=GameState::Defeated;'
)
foreach($token in $forbidden){if($code.Contains($token)){throw "Release invalido: todavia contiene $token"}}

Write-Host 'Fuente final validado: piso hexagonal continuo y texturizado, hexes transitables sin textura, NPC en tres carriles/campamentos, muro perimetral, combate animado y respawn automatico.'
