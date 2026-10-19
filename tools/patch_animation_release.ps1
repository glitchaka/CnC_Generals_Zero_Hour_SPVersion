param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function Replace-Required([string]$old,[string]$new,[string]$name){if(-not $script:code.Contains($old)){throw "No encontre: $name"};$script:code=$script:code.Replace($old,$new)}

Replace-Required 'std::vector<SkinnedMesh> meshes; std::vector<Bone> bones; std::unordered_map<std::string,int> boneByName; ID3D11ShaderResourceView* texture=nullptr;' 'std::vector<SkinnedMesh> meshes; std::vector<Bone> bones; std::unordered_map<std::string,int> boneByName; std::vector<XMMATRIX> poseWorld; std::string activeClip; ID3D11ShaderResourceView* texture=nullptr;' 'estado de pose para equipo'

Replace-Required 'static bool g_attackQueued=false,g_spaceLatch=false;' 'static bool g_attackQueued=false,g_spaceLatch=false; static float g_attackAnim=0.f,g_attackTotal=1.f,g_dodgeAnim=0.f,g_enemyAttackAnim=0.f,g_enemyAttackTotal=1.f; static bool g_attackDidHit=false,g_enemyAttackDidHit=false; static int g_attackVariant=0,g_dodgeVariant=0,g_locoVariant=0,g_npcLocoVariant=0; static float g_locoTimer=0.f,g_npcLocoTimer=0.f,g_idleTimer=0.f; static float g_dodgeDirX=0.f,g_dodgeDirZ=1.f;' 'estado de animaciones de combate'

Replace-Required 'auto itc=g_clips.find(clipName); if(itc==g_clips.end()||c.bones.empty())return; const AnimClip& clip=itc->second; c.animTime+=dt*rate; float t=clip.duration>0?fmodf(c.animTime,clip.duration):0;' 'auto itc=g_clips.find(clipName); if(itc==g_clips.end()||c.bones.empty())return; const AnimClip& clip=itc->second; if(c.activeClip!=clipName){c.activeClip=clipName;c.animTime=0.f;} c.animTime+=dt*rate; bool loop=(c.activeClip.rfind("Idle_",0)==0||c.activeClip.rfind("Walking_",0)==0||c.activeClip.rfind("Running_",0)==0||c.activeClip=="Melee_2H_Idle"); float t=clip.duration>0?(loop?fmodf(c.animTime,clip.duration):(std::min)(c.animTime,clip.duration-.0001f)):0;' 'reproduccion one-shot y cambio de clip'
Replace-Required 'for(size_t i=0;i<c.bones.size();++i)skin[i]=c.bones[i].invBind*calc(calc,(int)i);' 'for(size_t i=0;i<c.bones.size();++i)skin[i]=c.bones[i].invBind*calc(calc,(int)i); c.poseWorld=world;' 'guardar matrices de huesos'

$old=@'
static void updatePlayer(float dt){
    g_attackCooldown=(std::max)(0.f,g_attackCooldown-dt);g_dashCooldown=(std::max)(0.f,g_dashCooldown-dt);g_damageFlash=(std::max)(0.f,g_damageFlash-dt*2.8f);g_enemyHitFlash=(std::max)(0.f,g_enemyHitFlash-dt*4.f);
    if(g_gameState!=GameState::Playing){g_player.moving=false;poseCharacter(g_player,"Idle_A",dt);g_attackQueued=false;return;}
    float side=(g_keys['D']?1.f:0.f)-(g_keys['A']?1.f:0.f),fwd=(g_keys['W']?1.f:0.f)-(g_keys['S']?1.f:0.f); float m=sqrtf(side*side+fwd*fwd); if(m>1){side/=m;fwd/=m;}
    float dx=cosf(g_camYaw)*side+sinf(g_camYaw)*fwd,dz=-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd; float dm=sqrtf(dx*dx+dz*dz); bool run=g_keys[VK_SHIFT]; g_player.moving=dm>.01f; g_player.speed=g_player.moving?(run?5.6f:3.35f):0.f;
    if(g_player.moving){dx/=dm;dz/=dm;g_player.yaw=approachAngle(g_player.yaw,atan2f(dx,dz),12.f*dt);g_player.pos.x+=dx*g_player.speed*dt;g_player.pos.z+=dz*g_player.speed*dt;}
    bool space=g_keys[VK_SPACE];
    if(space&&!g_spaceLatch&&g_dashCooldown<=0.f){float ddx=dx,ddz=dz;if(dm<=.01f){ddx=sinf(g_player.yaw);ddz=cosf(g_player.yaw);}g_player.pos.x+=ddx*2.15f;g_player.pos.z+=ddz*2.15f;g_dashCooldown=1.15f;}
    g_spaceLatch=space;
    if(g_attackQueued&&g_attackCooldown<=0.f){
        float ex=g_npc.pos.x-g_player.pos.x,ez=g_npc.pos.z-g_player.pos.z,d=sqrtf(ex*ex+ez*ez);g_attackCooldown=.48f;
        if(d<2.15f&&g_enemyHealth>0.f){float nx=ex/(std::max)(d,.001f),nz=ez/(std::max)(d,.001f);g_player.yaw=atan2f(nx,nz);g_enemyHealth=(std::max)(0.f,g_enemyHealth-.26f);g_npc.pos.x+=nx*.72f;g_npc.pos.z+=nz*.72f;g_enemyHitFlash=.28f;if(g_enemyHealth<=0.f)g_gameState=GameState::Victory;}
    }
    g_attackQueued=false;resolveCharacter(g_player);
    poseCharacter(g_player,g_player.moving?(run?"Running_A":"Walking_A"):"Idle_A",dt,g_player.moving?(run?1.05f:1.f):1.f);
}
'@
$new=@'
static void updatePlayer(float dt){
    g_attackCooldown=(std::max)(0.f,g_attackCooldown-dt);g_dashCooldown=(std::max)(0.f,g_dashCooldown-dt);g_damageFlash=(std::max)(0.f,g_damageFlash-dt*2.8f);g_enemyHitFlash=(std::max)(0.f,g_enemyHitFlash-dt*4.f);g_locoTimer+=dt;g_idleTimer+=dt;
    if(g_gameState!=GameState::Playing){g_player.moving=false;poseCharacter(g_player,g_gameState==GameState::Defeated?"Death_A":"Idle_B",dt);g_attackQueued=false;return;}
    float side=(g_keys['D']?1.f:0.f)-(g_keys['A']?1.f:0.f),fwd=(g_keys['W']?1.f:0.f)-(g_keys['S']?1.f:0.f);float m=sqrtf(side*side+fwd*fwd);if(m>1){side/=m;fwd/=m;}
    float dx=cosf(g_camYaw)*side+sinf(g_camYaw)*fwd,dz=-sinf(g_camYaw)*side+cosf(g_camYaw)*fwd;float dm=sqrtf(dx*dx+dz*dz);if(dm>.01f){dx/=dm;dz/=dm;}bool run=g_keys[VK_SHIFT];
    bool space=g_keys[VK_SPACE];if(space&&!g_spaceLatch&&g_dashCooldown<=0.f&&g_attackAnim<=0.f){if(dm<=.01f){dx=sinf(g_player.yaw);dz=cosf(g_player.yaw);}g_dodgeDirX=dx;g_dodgeDirZ=dz;g_dodgeAnim=.40f;g_dashCooldown=1.05f;if(fwd<-.25f)g_dodgeVariant=3;else if(side<-.25f)g_dodgeVariant=1;else if(side>.25f)g_dodgeVariant=2;else g_dodgeVariant=0;}g_spaceLatch=space;
    if(g_attackQueued&&g_attackCooldown<=0.f&&g_dodgeAnim<=0.f&&g_attackAnim<=0.f){static const float dur[4]={1.07f,1.00f,1.37f,1.60f};g_attackVariant=(g_attackVariant+1)%4;g_attackTotal=dur[g_attackVariant];g_attackAnim=g_attackTotal;g_attackCooldown=g_attackTotal*.86f;g_attackDidHit=false;float ex=g_npc.pos.x-g_player.pos.x,ez=g_npc.pos.z-g_player.pos.z,d=sqrtf(ex*ex+ez*ez);if(d>.001f&&d<3.2f)g_player.yaw=atan2f(ex/d,ez/d);}g_attackQueued=false;
    if(g_dodgeAnim>0.f){g_dodgeAnim=(std::max)(0.f,g_dodgeAnim-dt);g_player.pos.x+=g_dodgeDirX*6.1f*dt;g_player.pos.z+=g_dodgeDirZ*6.1f*dt;static const char* dc[4]={"Dodge_Forward","Dodge_Left","Dodge_Right","Dodge_Backward"};poseCharacter(g_player,dc[g_dodgeVariant],dt);resolveCharacter(g_player);return;}
    if(g_attackAnim>0.f){g_attackAnim=(std::max)(0.f,g_attackAnim-dt);float phase=1.f-g_attackAnim/(std::max)(.001f,g_attackTotal);static const char* ac[4]={"Melee_1H_Attack_Chop","Melee_1H_Attack_Slice_Diagonal","Melee_1H_Attack_Slice_Horizontal","Melee_1H_Attack_Stab"};poseCharacter(g_player,ac[g_attackVariant],dt);if(!g_attackDidHit&&phase>=.40f){g_attackDidHit=true;float ex=g_npc.pos.x-g_player.pos.x,ez=g_npc.pos.z-g_player.pos.z,d=sqrtf(ex*ex+ez*ez);if(d<2.25f&&g_enemyHealth>0.f){float nx=ex/(std::max)(d,.001f),nz=ez/(std::max)(d,.001f);g_enemyHealth=(std::max)(0.f,g_enemyHealth-.26f);g_npc.pos.x+=nx*.55f;g_npc.pos.z+=nz*.55f;g_enemyHitFlash=.28f;if(g_enemyHealth<=0.f)g_gameState=GameState::Victory;}}resolveCharacter(g_player);return;}
    g_player.moving=dm>.01f;g_player.speed=g_player.moving?(run?5.6f:3.35f):0.f;if(g_player.moving){g_player.yaw=approachAngle(g_player.yaw,atan2f(dx,dz),12.f*dt);g_player.pos.x+=dx*g_player.speed*dt;g_player.pos.z+=dz*g_player.speed*dt;if(g_locoTimer>1.7f){g_locoTimer=0.f;g_locoVariant=(g_locoVariant+1)%3;}}
    resolveCharacter(g_player);if(g_player.moving){if(run)poseCharacter(g_player,(g_locoVariant&1)?"Running_B":"Running_A",dt,1.02f);else{static const char* wc[3]={"Walking_A","Walking_B","Walking_C"};poseCharacter(g_player,wc[g_locoVariant],dt,1.f);}}else{if(g_idleTimer>4.2f){g_idleTimer=0.f;g_locoVariant=(g_locoVariant+1)%2;}poseCharacter(g_player,(g_locoVariant&1)?"Idle_B":"Idle_A",dt);}
}
'@
Replace-Required $old $new 'movimiento y ataque animados del jugador'

$old=@'
static void updateNPC(float dt){
    if(g_gameState!=GameState::Playing||g_enemyHealth<=0.f){g_npc.moving=false;poseCharacter(g_npc,"Idle_A",dt);return;}
    g_enemyAttackCooldown=(std::max)(0.f,g_enemyAttackCooldown-dt);float dx=g_player.pos.x-g_npc.pos.x,dz=g_player.pos.z-g_npc.pos.z,d=sqrtf(dx*dx+dz*dz);g_npc.moving=g_npc.moving?(d>1.20f):(d>1.38f);g_npc.speed=g_npc.moving?2.75f:0.f;
    if(d>.001f){dx/=d;dz/=d;g_npc.yaw=approachAngle(g_npc.yaw,atan2f(dx,dz),7.f*dt);}
    if(g_npc.moving){g_npc.pos.x+=dx*g_npc.speed*dt;g_npc.pos.z+=dz*g_npc.speed*dt;}
    else if(d<1.5f&&g_enemyAttackCooldown<=0.f){g_enemyAttackCooldown=.82f;g_player.health=(std::max)(0.f,g_player.health-.13f);g_player.pos.x+=dx*.55f;g_player.pos.z+=dz*.55f;g_damageFlash=.34f;if(g_player.health<=0.f)g_gameState=GameState::Defeated;}
    resolveCharacter(g_npc);poseCharacter(g_npc,g_npc.moving?"Running_A":"Idle_A",dt,g_npc.moving?.82f:1.f);
}
'@
$new=@'
static void updateNPC(float dt){
    if(g_enemyHealth<=0.f){g_npc.moving=false;poseCharacter(g_npc,"Death_A",dt);return;}if(g_gameState!=GameState::Playing){g_npc.moving=false;poseCharacter(g_npc,"Melee_2H_Idle",dt);return;}
    g_enemyAttackCooldown=(std::max)(0.f,g_enemyAttackCooldown-dt);g_npcLocoTimer+=dt;float dx=g_player.pos.x-g_npc.pos.x,dz=g_player.pos.z-g_npc.pos.z,d=sqrtf(dx*dx+dz*dz);if(d>.001f){dx/=d;dz/=d;g_npc.yaw=approachAngle(g_npc.yaw,atan2f(dx,dz),7.f*dt);}
    if(g_enemyAttackAnim>0.f){g_enemyAttackAnim=(std::max)(0.f,g_enemyAttackAnim-dt);float phase=1.f-g_enemyAttackAnim/(std::max)(.001f,g_enemyAttackTotal);poseCharacter(g_npc,"Melee_2H_Attack_Chop",dt);if(!g_enemyAttackDidHit&&phase>=.48f){g_enemyAttackDidHit=true;if(d<1.7f){g_player.health=(std::max)(0.f,g_player.health-.13f);g_player.pos.x+=dx*.45f;g_player.pos.z+=dz*.45f;g_damageFlash=.34f;if(g_player.health<=0.f)g_gameState=GameState::Defeated;}}resolveCharacter(g_npc);return;}
    g_npc.moving=g_npc.moving?(d>1.28f):(d>1.48f);g_npc.speed=g_npc.moving?2.75f:0.f;if(g_npc.moving){g_npc.pos.x+=dx*g_npc.speed*dt;g_npc.pos.z+=dz*g_npc.speed*dt;if(g_npcLocoTimer>2.0f){g_npcLocoTimer=0.f;g_npcLocoVariant^=1;}resolveCharacter(g_npc);poseCharacter(g_npc,g_npcLocoVariant?"Running_B":"Running_A",dt,.86f);return;}
    if(d<1.55f&&g_enemyAttackCooldown<=0.f){g_enemyAttackTotal=1.63f;g_enemyAttackAnim=g_enemyAttackTotal;g_enemyAttackCooldown=1.38f;g_enemyAttackDidHit=false;poseCharacter(g_npc,"Melee_2H_Attack_Chop",0.f);return;}resolveCharacter(g_npc);poseCharacter(g_npc,"Melee_2H_Idle",dt);
}
'@
Replace-Required $old $new 'ataque animado y locomocion NPC'

$old='if(!loadAnimationClips("ExternalAssets/KayKitAdventurers/Rig_Medium_General.glb",{"Idle_A"})||!loadAnimationClips("ExternalAssets/KayKitAdventurers/Rig_Medium_MovementBasic.glb",{"Walking_A","Running_A"})){MessageBoxW(g_hwnd,L"No pude cargar las animaciones KayKit.",L"SPVersion",MB_ICONERROR);return 5;}'
$new='if(!loadAnimationClips("ExternalAssets/KayKitAdventurers/Rig_Medium_General.glb",{"Idle_A","Idle_B","Hit_A","Death_A"})||!loadAnimationClips("ExternalAssets/KayKitAdventurers/Rig_Medium_MovementBasic.glb",{"Walking_A","Walking_B","Walking_C","Running_A","Running_B"})||!loadAnimationClips("ExternalAssets/KayKitAdventurers/Rig_Medium_MovementAdvanced.glb",{"Dodge_Forward","Dodge_Left","Dodge_Right","Dodge_Backward"})||!loadAnimationClips("ExternalAssets/KayKitAdventurers/Rig_Medium_CombatMelee.glb",{"Melee_1H_Attack_Chop","Melee_1H_Attack_Slice_Diagonal","Melee_1H_Attack_Slice_Horizontal","Melee_1H_Attack_Stab","Melee_2H_Attack_Chop","Melee_2H_Idle"})){MessageBoxW(g_hwnd,L"No pude cargar el set completo de animaciones de combate KayKit.",L"SPVersion",MB_ICONERROR);return 5;}'
Replace-Required $old $new 'carga de clips de combate y locomocion'

$old=@'
static XMMATRIX actorWorld(const Character& c){return XMMatrixScaling(1.05f,1.05f,1.05f)*XMMatrixRotationY(c.yaw)*XMMatrixTranslation(c.pos.x,c.pos.y,c.pos.z);}
static void drawCharacter(const Character& c){XMMATRIX actor=actorWorld(c);for(const auto& m:c.meshes)drawMeshWorld(m.gpu,actor,{1,1,1,.5f},1,c.texture);}
static void drawEquipment(const Character& c,const MeshGPU& sword,const MeshGPU* shield,bool player){
    XMMATRIX actor=actorWorld(c);float swing=0.f;if(player&&g_attackCooldown>0.f){float p=1.f-clampf(g_attackCooldown/.48f,0.f,1.f);swing=-1.15f+p*2.25f;}
    XMMATRIX sw=XMMatrixScaling(.72f,.72f,.72f)*XMMatrixRotationX(XM_PIDIV2)*XMMatrixRotationZ(-.28f+swing)*XMMatrixTranslation(.43f,1.03f,.12f)*actor;drawMeshWorld(sword,sw,{.84f,.88f,.92f,.5f},0,nullptr);
    if(shield){XMMATRIX sh=XMMatrixScaling(.72f,.72f,.72f)*XMMatrixRotationY(XM_PIDIV2)*XMMatrixTranslation(-.42f,.96f,.10f)*actor;drawMeshWorld(*shield,sh,{.48f,.58f,.72f,.5f},0,nullptr);}
}
'@
$new=@'
static XMMATRIX actorWorld(const Character& c){return XMMatrixScaling(1.05f,1.05f,1.05f)*XMMatrixRotationY(c.yaw)*XMMatrixTranslation(c.pos.x,c.pos.y,c.pos.z);}
static void drawCharacter(const Character& c){XMMATRIX actor=actorWorld(c);for(const auto& m:c.meshes)drawMeshWorld(m.gpu,actor,{1,1,1,.5f},1,c.texture);}
static int findHandBone(const Character& c,bool right){int fallback=-1;for(size_t i=0;i<c.bones.size();++i){std::string n=c.bones[i].name;std::transform(n.begin(),n.end(),n.begin(),[](unsigned char q){return (char)tolower(q);});if(n.find("hand")!=std::string::npos){if(fallback<0)fallback=(int)i;bool r=n.find("right")!=std::string::npos||n.find(".r")!=std::string::npos||n.find("_r")!=std::string::npos;bool l=n.find("left")!=std::string::npos||n.find(".l")!=std::string::npos||n.find("_l")!=std::string::npos;if((right&&r)||(!right&&l))return (int)i;}}return fallback;}
static void drawEquipment(const Character& c,const MeshGPU& sword,const MeshGPU* shield,bool player){XMMATRIX actor=actorWorld(c);int rh=findHandBone(c,true);XMMATRIX hand=(rh>=0&&rh<(int)c.poseWorld.size())?c.poseWorld[rh]:XMMatrixTranslation(.43f,1.03f,.12f);XMMATRIX sw=XMMatrixScaling(.68f,.68f,.68f)*XMMatrixRotationX(XM_PIDIV2)*XMMatrixRotationZ(-.25f)*hand*actor;drawMeshWorld(sword,sw,{.84f,.88f,.92f,.5f},0,nullptr);if(shield){int lh=findHandBone(c,false);XMMATRIX h=(lh>=0&&lh<(int)c.poseWorld.size())?c.poseWorld[lh]:XMMatrixTranslation(-.42f,.96f,.10f);XMMATRIX sh=XMMatrixScaling(.70f,.70f,.70f)*XMMatrixRotationY(XM_PIDIV2)*h*actor;drawMeshWorld(*shield,sh,{.48f,.58f,.72f,.5f},0,nullptr);}}
'@
Replace-Required $old $new 'arma unida al hueso sin giro procedural'

Replace-Required 'g_gameState=GameState::Playing;g_player.health=1.f;g_enemyHealth=1.f;g_attackCooldown=g_dashCooldown=g_enemyAttackCooldown=g_damageFlash=g_enemyHitFlash=0.f;g_attackQueued=false;g_spaceLatch=false;' 'g_gameState=GameState::Playing;g_player.health=1.f;g_enemyHealth=1.f;g_attackCooldown=g_dashCooldown=g_enemyAttackCooldown=g_damageFlash=g_enemyHitFlash=0.f;g_attackAnim=g_dodgeAnim=g_enemyAttackAnim=0.f;g_attackDidHit=g_enemyAttackDidHit=false;g_attackQueued=false;g_spaceLatch=false;g_player.activeClip.clear();g_npc.activeClip.clear();g_locoTimer=g_npcLocoTimer=g_idleTimer=0.f;' 'reset de animaciones'

if($code -eq $original){throw 'El parche de animaciones no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host "Animaciones KayKit de combate y locomocion aplicadas: $SourcePath"
