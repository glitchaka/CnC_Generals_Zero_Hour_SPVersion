param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

RR 'const int N=221; const float SIZE=340.f' 'const int N=241; const float SIZE=560.f' 'suelo sin horizonte gris'
RR 'static MeshGPU *g_blueTowerMesh=nullptr,*g_redTowerMesh=nullptr,*g_blueCoreMesh=nullptr,*g_redCoreMesh=nullptr;' 'static MeshGPU *g_blueTowerMesh=nullptr,*g_redTowerMesh=nullptr,*g_blueCoreMesh=nullptr,*g_redCoreMesh=nullptr,*g_boundaryWallMesh=nullptr; struct Mob{XMFLOAT3 pos{0,0,0};float yaw=0,health=1.f,attackCd=0;bool alive=true;}; static std::array<Mob,8> g_mobs; static int g_mobsRemaining=8;' 'enemigos y muro perimetral'
RR 'if(p.find(L"building_castle_red.obj")!=std::wstring::npos||p.find(L"building_townhall_red.obj")!=std::wstring::npos)g_redCoreMesh=m;' 'if(p.find(L"building_castle_red.obj")!=std::wstring::npos||p.find(L"building_townhall_red.obj")!=std::wstring::npos)g_redCoreMesh=m; if(!g_boundaryWallMesh&&(p.find(L"building_wall")!=std::wstring::npos||p.find(L"wall_straight")!=std::wstring::npos))g_boundaryWallMesh=m;' 'seleccion de muro exterior'
RR 'target=p.find(L"tower")!=std::wstring::npos?4.3f:5.2f;' 'target=p.find(L"tower")!=std::wstring::npos?9.0f:11.5f;' 'escala real de edificios'
RR 'addSceneMesh(g_blueCoreMesh,-106.f,-106.f,.78f,7.0f,true,g_medievalSRV);' 'addSceneMesh(g_blueCoreMesh,-106.f,-106.f,.78f,15.0f,true,g_medievalSRV);' 'nucleo azul grande'
RR 'addSceneMesh(g_redCoreMesh,106.f,106.f,-2.35f,7.0f,true,g_medievalSRV);' 'addSceneMesh(g_redCoreMesh,106.f,106.f,-2.35f,15.0f,true,g_medievalSRV);' 'nucleo rojo grande'
RR 'addSceneMesh(m,p.x,p.z,yaw,4.6f,true,g_medievalSRV);' 'addSceneMesh(m,p.x,p.z,yaw,8.6f,true,g_medievalSRV);' 'torres grandes'

$anchor='static bool buildProfessionalMap(){'
$insert=@'
static void buildBoundaryWalls(){
    if(!g_boundaryWallMesh)return;const float B=118.f;const int N=22;for(int i=0;i<=N;++i){float t=-B+(2.f*B*i/N);addSceneMesh(g_boundaryWallMesh,t,-B,0,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,t,B,XM_PI,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,-B,t,XM_PIDIV2,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,B,t,-XM_PIDIV2,7.5f,true,g_medievalSRV);}}
static void resetMobWave(){
    const XMFLOAT3 p[8]={{-48,0,-48},{-24,0,-24},{18,0,18},{44,0,44},{-96,0,10},{10,0,-96},{96,0,-10},{-10,0,96}};g_mobsRemaining=8;for(int i=0;i<8;++i){g_mobs[i].pos={p[i].x,groundH(p[i].x,p[i].z),p[i].z};g_mobs[i].yaw=0;g_mobs[i].health=1.f;g_mobs[i].attackCd=0;g_mobs[i].alive=true;}}
static void updateMobs(float dt){
    if(g_gameState!=GameState::Playing)return;for(auto& m:g_mobs){if(!m.alive)continue;m.attackCd=(std::max)(0.f,m.attackCd-dt);float dx=g_player.pos.x-m.pos.x,dz=g_player.pos.z-m.pos.z,d=sqrtf(dx*dx+dz*dz);if(d>.01f){dx/=d;dz/=d;m.yaw=approachAngle(m.yaw,atan2f(dx,dz),6.f*dt);}if(d<12.f&&d>1.45f){m.pos.x+=dx*2.15f*dt;m.pos.z+=dz*2.15f*dt;}else if(d<=1.45f&&m.attackCd<=0.f){m.attackCd=1.05f;g_player.health=(std::max)(0.f,g_player.health-.08f);g_damageFlash=.24f;if(g_player.health<=0.f){g_player.health=1.f;g_player.pos={-92.f,groundH(-92.f,-92.f),-92.f};g_player.home=g_player.pos;g_damageFlash=.65f;}}m.pos.y=groundH(m.pos.x,m.pos.z);}}
static void damageNearestMob(){float best=2.4f;Mob* hit=nullptr;for(auto& m:g_mobs){if(!m.alive)continue;float dx=m.pos.x-g_player.pos.x,dz=m.pos.z-g_player.pos.z,d=sqrtf(dx*dx+dz*dz);if(d<best){best=d;hit=&m;}}if(hit){hit->health-=.34f;if(hit->health<=0){hit->alive=false;g_mobsRemaining=(std::max)(0,g_mobsRemaining-1);}}}

static bool buildProfessionalMap(){
'@
RR $anchor $insert 'gameplay del mapa'

RR 'buildLaneRibbon(laneTop(),0);buildLaneRibbon(laneMid(),1);buildLaneRibbon(laneBot(),2);buildRiverRibbon();buildBridges();buildStrategicStructures();buildJungleWalls();' 'buildLaneRibbon(laneTop(),0);buildLaneRibbon(laneMid(),1);buildLaneRibbon(laneBot(),2);buildRiverRibbon();buildBridges();buildStrategicStructures();buildJungleWalls();buildBoundaryWalls();resetMobWave();' 'muro perimetral y oleada'
RR 'g_blueTowerMesh=g_redTowerMesh=g_blueCoreMesh=g_redCoreMesh=nullptr;' 'g_blueTowerMesh=g_redTowerMesh=g_blueCoreMesh=g_redCoreMesh=g_boundaryWallMesh=nullptr;' 'reset muro'

RR 'if(!g_attackDidHit&&phase>=.40f){g_attackDidHit=true;' 'if(!g_attackDidHit&&phase>=.40f){g_attackDidHit=true;damageNearestMob();' 'ataque contra mobs'
RR 'updateNPC(dt);resolveCharacterPair(g_player,g_npc);updateCamera(dt);' 'updateNPC(dt);updateMobs(dt);resolveCharacterPair(g_player,g_npc);updateCamera(dt);' 'actualizacion de mobs'

$renderAnchor='static void render(){'
$renderHelpers=@'
static XMMATRIX mobWorld(const Mob& m){return XMMatrixScaling(.92f,.92f,.92f)*XMMatrixRotationY(m.yaw)*XMMatrixTranslation(m.pos.x,m.pos.y,m.pos.z);}
static void drawMobs(){for(const auto& mob:g_mobs){if(!mob.alive)continue;XMMATRIX actor=mobWorld(mob);for(const auto& sm:g_npc.meshes)drawMeshWorld(sm.gpu,actor,{1,1,1,.5f},1,g_npc.texture);}}

static void render(){
'@
RR $renderAnchor $renderHelpers 'render de mobs despues de drawMeshWorld'
RR 'drawCharacter(g_npc);drawEquipment(g_npc,g_npcSword,nullptr,false);drawCharacter(g_player);' 'drawMobs();drawCharacter(g_npc);drawEquipment(g_npc,g_npcSword,nullptr,false);drawCharacter(g_player);' 'render de mobs'

# Elimina cualquier ruta restante que mande a pantalla de derrota: muerte significa respawn.
$code=$code.Replace('if(g_player.health<=0.f)g_gameState=GameState::Defeated;','if(g_player.health<=0.f){g_player.health=1.f;g_player.pos={-92.f,groundH(-92.f,-92.f),-92.f};g_player.home=g_player.pos;g_damageFlash=.65f;}')

if($code -eq $original){throw 'El parche de gameplay no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Gameplay final aplicado: enemigos, combate, respawn, edificios a escala, muro perimetral y suelo extendido.'
