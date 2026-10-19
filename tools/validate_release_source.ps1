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
  'buildHexFloor()',
  'loadFloorTileCPU',
  'appendFloorTile',
  'hex_grass.obj',
  'const float tileW=(maxX-minX)*S',
  'const float tileH=(maxZ-minZ)*S',
  'const float row=tileH*.75f',
  'v.uv={x*.045f,z*.045f}',
  'blocksGameplay(cx,cz,radius)',
  'drawMeshWorld(g_hexFloorTextured,XMMatrixIdentity(),{1,1,1,1},1,g_groundSRV)',
  'drawMeshWorld(g_hexFloorTransit',
  'addSceneMeshNatural',
  '1.0f,coll,tex,tint,yoff',
  'g_boundaryWallMesh',
  'const int N=80',
  'g_dungeonFullSRV',
  'clampf(c.pos.x,-113.5f,113.5f)',
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
  'drawMeshWorld(g_terrain,XMMatrixIdentity(),{.92f,.92f,.92f,1},1,g_groundSRV);drawMeshWorld(g_hexFloorTextured',
  'R - REINICIAR',
  'Victory&&keyPressed(''R'')',
  'Victory && keyPressed(''R'')',
  '392.f*g_enemyHealth',
  'if(g_player.health<=0.f)g_gameState=GameState::Defeated;'
)
foreach($token in $forbidden){if($code.Contains($token)){throw "Release invalido: todavia contiene $token"}}

Write-Host 'Fuente final validado: piso visible hecho con hex_grass calzado por su huella real, textura de suelo por UV mundial, transito con la misma geometria sin textura, construcciones a escala natural y victoria sin reinicio.'
