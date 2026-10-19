param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

RR 'const int N=221; const float SIZE=340.f' 'const int N=241; const float SIZE=560.f' 'suelo sin horizonte gris'
RR 'static MeshGPU *g_blueTowerMesh=nullptr,*g_redTowerMesh=nullptr,*g_blueCoreMesh=nullptr,*g_redCoreMesh=nullptr;' 'static MeshGPU *g_blueTowerMesh=nullptr,*g_redTowerMesh=nullptr,*g_blueCoreMesh=nullptr,*g_redCoreMesh=nullptr,*g_boundaryWallMesh=nullptr; struct Mob{XMFLOAT3 pos{0,0,0};float yaw=0,health=1.f,attackCd=0,attackAnim=0,progress=.9f;int lane=1;bool alive=true,camp=false;}; static std::array<Mob,12> g_mobs; static int g_mobsRemaining=12; static int g_waveNumber=1; static float g_waveDelay=0.f;' 'enemigos y muro perimetral'
RR 'if(p.find(L"building_castle_red.obj")!=std::wstring::npos||p.find(L"building_townhall_red.obj")!=std::wstring::npos)g_redCoreMesh=m;' 'if(p.find(L"building_castle_red.obj")!=std::wstring::npos||p.find(L"building_townhall_red.obj")!=std::wstring::npos)g_redCoreMesh=m; if(!g_boundaryWallMesh&&(p.find(L"building_wall")!=std::wstring::npos||p.find(L"wall_straight")!=std::wstring::npos))g_boundaryWallMesh=m;' 'seleccion de muro exterior'

# Los edificios del pack tienen una huella horizontal muy grande. Escalarlos por la dimension maxima
# los dejaba absurdamente bajos frente al personaje. Para piezas grandes usamos altura real.
RR 'static float meshScale(const MeshGPU& m,float target){float ex=m.bmax.x-m.bmin.x,ey=m.bmax.y-m.bmin.y,ez=m.bmax.z-m.bmin.z,e=(std::max)(.01f,(std::max)(ex,(std::max)(ey,ez)));return target/e;}' 'static float meshScale(const MeshGPU& m,float target){float ex=m.bmax.x-m.bmin.x,ey=m.bmax.y-m.bmin.y,ez=m.bmax.z-m.bmin.z;float raw=target>=8.f?ey:(std::max)(ex,(std::max)(ey,ez));float e=(std::max)(.01f,raw);return target/e;}' 'escala por altura para edificios'
RR 'target=p.find(L"tower")!=std::wstring::npos?4.3f:5.2f;' 'target=p.find(L"tower")!=std::wstring::npos?12.0f:16.0f;' 'escala real de edificios de base'
RR 'addSceneMesh(g_blueCoreMesh,-106.f,-106.f,.78f,7.0f,true,g_medievalSRV);' 'addSceneMesh(g_blueCoreMesh,-106.f,-106.f,.78f,20.0f,true,g_medievalSRV);' 'nucleo azul grande'
RR 'addSceneMesh(g_redCoreMesh,106.f,106.f,-2.35f,7.0f,true,g_medievalSRV);' 'addSceneMesh(g_redCoreMesh,106.f,106.f,-2.35f,20.0f,true,g_medievalSRV);' 'nucleo rojo grande'
RR 'addSceneMesh(m,p.x,p.z,yaw,4.6f,true,g_medievalSRV);' 'addSceneMesh(m,p.x,p.z,yaw,12.0f,true,g_medievalSRV);' 'torres grandes'

$anchor='static bool buildProfessionalMap(){'
$insert=@'
static void buildBoundaryWalls(){
    if(!g_boundaryWallMesh)return;const float B=118.f;const int N=52;for(int i=0;i<=N;++i){float t=-B+(2.f*B*i/N);addSceneMesh(g_boundaryWallMesh,t,-B,0,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,t,B,XM_PI,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,-B,t,XM_PIDIV2,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,B,t,-XM_PIDIV2,7.5f,true,g_medievalSRV);}}
static const std::vector<MapPt>& mobLane(int lane){return lane==0?laneTop():(lane==2?laneBot():laneMid());}
static void spawnMob(int i){
    static const int lanes[12]={0,0,0,1,1,1,2,2,2,-1,-1,-1};
    static const float prog[12]={.93f,.82f,.71f,.94f,.83f,.72f,.93f,.82f,.71f,.10f,.43f,.76f};
    Mob& m=g_mobs[i];m.lane=lanes[i];m.camp=m.lane<0;m.progress=prog[i];m.health=1.f;m.attackCd=0;m.attackAnim=0;m.alive=true;
    float yaw=0;if(!m.camp){MapPt p=samplePolyline(mobLane(m.lane),m.progress,yaw);m.pos={p.x,groundH(p.x,p.z),p.z};m.yaw=yaw;}
    else{const auto& cc=camps();MapPt c=cc[(i*3)%cc.size()];float a=m.progress*XM_2PI;m.pos={c.x+cosf(a)*3.4f,0,c.z+sinf(a)*3.4f};m.pos.y=groundH(m.pos.x,m.pos.z);m.yaw=-a;}
}
static void resetMobWave(){g_mobsRemaining=12;g_waveDelay=0.f;for(int i=0;i<12;++i)spawnMob(i);}
static void resolvePlayerMobCollision(){
    const float minD=1.18f;for(auto& m:g_mobs){if(!m.alive)continue;float dx=g_player.pos.x-m.pos.x,dz=g_player.pos.z-m.pos.z,ds=dx*dx+dz*dz;if(ds>=minD*minD)continue;if(ds<1e-6f){dx=1.f;dz=0.f;ds=1.f;}float d=sqrtf(ds),push=minD-d;g_player.pos.x+=dx/d*push;g_player.pos.z+=dz/d*push;}resolveCharacter(g_player);}
static void updateMobs(float dt){
    if(g_gameState!=GameState::Playing)g_gameState=GameState::Playing;
    if(g_mobsRemaining<=0){g_waveDelay+=dt;poseCharacter(g_npc,"Idle_A",dt);if(g_waveDelay>=1.65f){++g_waveNumber;resetMobWave();}return;}
    bool anyAttack=false;
    for(int i=0;i<12;++i){Mob& m=g_mobs[i];if(!m.alive)continue;m.attackCd=(std::max)(0.f,m.attackCd-dt);m.attackAnim=(std::max)(0.f,m.attackAnim-dt);anyAttack=anyAttack||m.attackAnim>0.f;
        float pdx=g_player.pos.x-m.pos.x,pdz=g_player.pos.z-m.pos.z,pd=sqrtf(pdx*pdx+pdz*pdz);bool aggro=pd<11.5f;float tx=m.pos.x,tz=m.pos.z;
        if(aggro){tx=g_player.pos.x;tz=g_player.pos.z;}
        else if(!m.camp){m.progress-=dt*(.0062f+.00015f*g_waveNumber);if(m.progress<.065f)m.progress=.94f;float ly=0;MapPt p=samplePolyline(mobLane(m.lane),m.progress,ly);tx=p.x;tz=p.z;}
        else{m.progress=fmodf(m.progress+dt*.045f,1.f);const auto& cc=camps();MapPt c=cc[(i*3)%cc.size()];float a=m.progress*XM_2PI;tx=c.x+cosf(a)*3.4f;tz=c.z+sinf(a)*3.4f;}
        float dx=tx-m.pos.x,dz=tz-m.pos.z,d=sqrtf(dx*dx+dz*dz);if(d>.01f){dx/=d;dz/=d;m.yaw=approachAngle(m.yaw,atan2f(dx,dz),6.f*dt);}if(aggro&&pd<=1.34f){if(m.attackCd<=0.f){m.attackCd=.92f;m.attackAnim=.52f;anyAttack=true;g_player.health=(std::max)(0.f,g_player.health-.075f);g_damageFlash=.24f;if(g_player.health<=0.f){g_player.health=1.f;g_player.pos={-92.f,groundH(-92.f,-92.f),-92.f};g_player.home=g_player.pos;g_player.yaw=.78f;g_attackAnim=g_dodgeAnim=0.f;g_attackQueued=false;g_damageFlash=.65f;}}}
        else if(d>.10f){float speed=aggro?(2.15f+.04f*g_waveNumber):1.72f;m.pos.x+=dx*speed*dt;m.pos.z+=dz*speed*dt;}m.pos.y=groundH(m.pos.x,m.pos.z);}
    poseCharacter(g_npc,anyAttack?"Melee_2H_Attack_Chop":"Running_A",dt,anyAttack?1.f:.86f);resolvePlayerMobCollision();}
static void damageNearestMob(){float best=2.55f;Mob* hit=nullptr;for(auto& m:g_mobs){if(!m.alive)continue;float dx=m.pos.x-g_player.pos.x,dz=m.pos.z-g_player.pos.z,d=sqrtf(dx*dx+dz*dz);if(d<best){best=d;hit=&m;}}if(hit){hit->health-=.34f;g_enemyHitFlash=.22f;if(hit->health<=0){hit->alive=false;g_mobsRemaining=(std::max)(0,g_mobsRemaining-1);}}}

static bool buildProfessionalMap(){
'@
RR $anchor $insert 'gameplay del mapa'

RR 'buildLaneRibbon(laneTop(),0);buildLaneRibbon(laneMid(),1);buildLaneRibbon(laneBot(),2);buildRiverRibbon();buildBridges();buildStrategicStructures();buildJungleWalls();' 'buildLaneRibbon(laneTop(),0);buildLaneRibbon(laneMid(),1);buildLaneRibbon(laneBot(),2);buildRiverRibbon();buildBridges();buildStrategicStructures();buildJungleWalls();buildBoundaryWalls();resetMobWave();' 'muro perimetral y oleada'
RR 'g_blueTowerMesh=g_redTowerMesh=g_blueCoreMesh=g_redCoreMesh=nullptr;' 'g_blueTowerMesh=g_redTowerMesh=g_blueCoreMesh=g_redCoreMesh=g_boundaryWallMesh=nullptr;' 'reset muro'

# El ataque golpea la oleada, no un enemigo oculto separado.
RR 'if(!g_attackDidHit&&phase>=.40f){g_attackDidHit=true;' 'if(!g_attackDidHit&&phase>=.40f){g_attackDidHit=true;damageNearestMob();' 'ataque contra mobs'
$oldHit='float ex=g_npc.pos.x-g_player.pos.x,ez=g_npc.pos.z-g_player.pos.z,d=sqrtf(ex*ex+ez*g_enemyHealth);'
# La expresion anterior nunca debe existir; mantenemos el reemplazo real por compatibilidad con el parche de animaciones.
$realHit='float ex=g_npc.pos.x-g_player.pos.x,ez=g_npc.pos.z-g_player.pos.z,d=sqrtf(ex*ex+ez*ez);if(d<2.25f&&g_enemyHealth>0.f){float nx=ex/(std::max)(d,.001f),nz=ez/(std::max)(d,.001f);g_enemyHealth=(std::max)(0.f,g_enemyHealth-.26f);g_npc.pos.x+=nx*.55f;g_npc.pos.z+=nz*.55f;g_enemyHitFlash=.28f;if(g_enemyHealth<=0.f)g_gameState=GameState::Victory;}'
if(-not $code.Contains($realHit)){throw 'No encontre el golpe del NPC unico.'}
$code=$code.Replace($realHit,'')
$oldFace='float ex=g_npc.pos.x-g_player.pos.x,ez=g_npc.pos.z-g_player.pos.z,d=sqrtf(ex*ex+ez*ez);if(d>.001f&&d<3.2f)g_player.yaw=atan2f(ex/d,ez/d);'
if(-not $code.Contains($oldFace)){throw 'No encontre el autoapuntado al NPC unico.'}
$code=$code.Replace($oldFace,'')

$loopNew=[regex]::Replace($code,'updateNPC\s*\(\s*dt\s*\)\s*;','updateMobs(dt);',1)
if($loopNew -eq $code){throw 'No encontre: llamada updateNPC(dt) del bucle principal'}
$code=$loopNew

$renderAnchor='static void render(){'
$renderHelpers=@'
static XMMATRIX mobWorld(const Mob& m){return XMMatrixScaling(1.05f,1.05f,1.05f)*XMMatrixRotationY(m.yaw)*XMMatrixTranslation(m.pos.x,m.pos.y,m.pos.z);}
static void drawMobs(){for(const auto& mob:g_mobs){if(!mob.alive)continue;XMMATRIX actor=mobWorld(mob);for(const auto& sm:g_npc.meshes)drawMeshWorld(sm.gpu,actor,{1,1,1,.5f},1,g_npc.texture);}}

static void render(){
'@
RR $renderAnchor $renderHelpers 'render de mobs despues de drawMeshWorld'
RR 'drawCharacter(g_npc);drawEquipment(g_npc,g_npcSword,nullptr,false);drawCharacter(g_player);' 'drawMobs();drawCharacter(g_player);' 'quitar NPC unico y renderizar oleada'

# HUD: representa la oleada completa, nunca una barra de vida del Ranger oculto.
RR 'drawText5x7("ENEMIGO",cx-62.f,34.f,2.f,{1,1,1,.96f});' 'drawText5x7("ENEMIGOS",cx-72.f,34.f,2.f,{1,1,1,.96f});' 'etiqueta de enemigos'
RR '392.f*g_enemyHealth' '392.f*(g_mobsRemaining/12.f)' 'barra de enemigos restantes'

# Ninguna ruta normal termina la partida. La muerte del jugador reaparece en base y al limpiar
# la oleada aparece la siguiente automaticamente.
$code=$code.Replace('if(g_player.health<=0.f)g_gameState=GameState::Defeated;','if(g_player.health<=0.f){g_player.health=1.f;g_player.pos={-92.f,groundH(-92.f,-92.f),-92.f};g_player.home=g_player.pos;g_damageFlash=.65f;}')
$victoryHud='else if(g_gameState==GameState::Victory){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.02f,.06f,.03f,.40f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("VICTORIA",cx-108.f,g_h*.5f-48.f,4.f,{.25f,.92f,.42f,1});drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});}'
if($code.Contains($victoryHud)){$code=$code.Replace($victoryHud,'')}

if($code -eq $original){throw 'El parche de gameplay no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Gameplay v7 aplicado: NPC en los tres carriles y campamentos, oleadas, persecucion, ataques animados, respawn y edificios a escala.'
