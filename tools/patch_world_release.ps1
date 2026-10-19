param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function R([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

R 'struct SceneObj { MeshGPU* mesh=nullptr; float x=0,z=0,yaw=0,scale=1; bool collider=true; };' 'struct SceneObj { MeshGPU* mesh=nullptr; float x=0,z=0,yaw=0,scale=1; bool collider=true; ID3D11ShaderResourceView* tex=nullptr; XMFLOAT4 tint{1,1,1,1}; float yOffset=0; };' 'SceneObj extendido'
R 'static std::vector<SceneObj> g_scene;' 'static std::vector<SceneObj> g_scene; static std::vector<MeshGPU*> g_worldOwned; static std::vector<XMFLOAT2> g_mapOccupied; static ID3D11ShaderResourceView *g_forestSRV=nullptr,*g_medievalSRV=nullptr,*g_dungeonFullSRV=nullptr;' 'recursos del mapa'
R 'static XMMATRIX objectWorld(const SceneObj& o){ return XMMatrixScaling(o.scale,o.scale,o.scale)*XMMatrixRotationY(o.yaw)*XMMatrixTranslation(o.x,groundH(o.x,o.z),o.z); }' 'static XMMATRIX objectWorld(const SceneObj& o){ return XMMatrixScaling(o.scale,o.scale,o.scale)*XMMatrixRotationY(o.yaw)*XMMatrixTranslation(o.x,groundH(o.x,o.z)+o.yOffset,o.z); }' 'altura de props'
R 'for(const auto& o:g_scene)drawMeshWorld(*o.mesh,objectWorld(o),{1,1,1,1},1,g_kaySRV);' 'for(const auto& o:g_scene)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);' 'texturas de escena'

$anchor='static void resolveCharacter(Character& c){'
$world=@'
static std::wstring lowerW(std::wstring s){for(auto& c:s){if(c>=L'A'&&c<=L'Z')c=(wchar_t)(c-L'A'+L'a');}return s;}
static void collectObjFiles(const std::wstring& root,std::vector<std::wstring>& out){WIN32_FIND_DATAW fd{};HANDLE h=FindFirstFileW((root+L"\\*").c_str(),&fd);if(h==INVALID_HANDLE_VALUE)return;do{if(fd.cFileName[0]==L'.')continue;std::wstring p=root+L"\\"+fd.cFileName;if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)collectObjFiles(p,out);else{std::wstring n=lowerW(fd.cFileName);if(n.size()>4&&n.substr(n.size()-4)==L".obj")out.push_back(p);}}while(FindNextFileW(h,&fd));FindClose(h);}
static uint32_t mapHash(const std::wstring& s){uint32_t h=2166136261u;for(wchar_t c:s){h^=(uint32_t)c;h*=16777619u;}return h;}
static float meshScale(const MeshGPU& m,float target){float ex=m.bmax.x-m.bmin.x,ey=m.bmax.y-m.bmin.y,ez=m.bmax.z-m.bmin.z,e=(std::max)(.01f,(std::max)(ex,(std::max)(ey,ez)));return target/e;}
static float segDist(float px,float pz,float ax,float az,float bx,float bz){float vx=bx-ax,vz=bz-az,wx=px-ax,wz=pz-az,den=vx*vx+vz*vz,t=den>1e-5f?clampf((wx*vx+wz*vz)/den,0.f,1.f):0.f;float dx=px-(ax+vx*t),dz=pz-(az+vz*t);return sqrtf(dx*dx+dz*dz);}
static float laneDistance(float x,float z){
    float d=segDist(x,z,-68.f,-68.f,68.f,68.f);
    d=(std::min)(d,segDist(x,z,-68.f,-68.f,-64.f,64.f));
    d=(std::min)(d,segDist(x,z,-64.f,64.f,64.f,64.f));
    d=(std::min)(d,segDist(x,z,64.f,64.f,68.f,68.f));
    d=(std::min)(d,segDist(x,z,-68.f,-68.f,64.f,-64.f));
    d=(std::min)(d,segDist(x,z,64.f,-64.f,64.f,64.f));
    d=(std::min)(d,segDist(x,z,64.f,64.f,68.f,68.f));
    return d;
}
static bool baseClear(float x,float z){float bx=x+68.f,bz=z+68.f,rx=x-68.f,rz=z-68.f;return bx*bx+bz*bz<17.f*17.f||rx*rx+rz*rz<17.f*17.f;}
static bool reservedPlayable(float x,float z){float river=segDist(x,z,-58.f,58.f,58.f,-58.f);return laneDistance(x,z)<10.5f||river<5.5f||baseClear(x,z);}
static bool occupiedNear(float x,float z,float r){float rr=r*r;for(const auto& p:g_mapOccupied){float dx=x-p.x,dz=z-p.y;if(dx*dx+dz*dz<rr)return true;}return false;}
static bool decorSpot(uint32_t h,size_t index,float& x,float& z,float gap){
    const int N=45;const float step=4.0f,start=-88.f;
    for(uint32_t a=0;a<2400;++a){uint32_t q=h+(uint32_t)index*2654435761u+a*2246822519u;int ix=(int)(q%N),iz=(int)((q/N)%N);x=start+ix*step;z=start+iz*step;if(reservedPlayable(x,z)||occupiedNear(x,z,gap))continue;g_mapOccupied.push_back({x,z});return true;}
    return false;
}
static void addScene(MeshGPU* m,float x,float z,float yaw,float target,bool collider,ID3D11ShaderResourceView* tex,float yoff=0.f,XMFLOAT4 tint={1,1,1,1}){g_scene.push_back({m,x,z,yaw,meshScale(*m,target),collider,tex,tint,yoff});}
static void buildLaneGeometry(MeshGPU* lane,MeshGPU* river,ID3D11ShaderResourceView* tex){
    if(lane){for(float t=-60.f;t<=60.f;t+=6.f){addScene(lane,t,t,XM_PIDIV4,5.6f,false,tex,.03f);addScene(lane,-64.f,t,0.f,5.6f,false,tex,.03f);addScene(lane,t,64.f,XM_PIDIV2,5.6f,false,tex,.03f);addScene(lane,t,-64.f,XM_PIDIV2,5.6f,false,tex,.03f);addScene(lane,64.f,t,0.f,5.6f,false,tex,.03f);}for(float t=0;t<=1.001f;t+=.2f){float x=-68.f+4.f*t,z=-68.f+132.f*t;addScene(lane,x,z,0.f,5.6f,false,tex,.03f);x=-68.f+132.f*t;z=-68.f+4.f*t;addScene(lane,x,z,XM_PIDIV2,5.6f,false,tex,.03f);x=64.f+4.f*t;z=64.f+4.f*t;addScene(lane,x,z,XM_PIDIV4,5.6f,false,tex,.03f);}}
    if(river){for(float t=-54.f;t<=54.f;t+=6.f)addScene(river,t,-t,-XM_PIDIV4,5.8f,false,tex,.015f,{.82f,.94f,1.f,1.f});}
}
static void placeFullPack(const std::wstring& root,int kind,ID3D11ShaderResourceView* tex){
    std::vector<std::wstring> files;collectObjFiles(root,files);MeshGPU* laneTile=nullptr;MeshGPU* riverTile=nullptr;
    for(size_t i=0;i<files.size();++i){MeshGPU* m=new MeshGPU;if(!loadOBJ(files[i],*m)){delete m;continue;}g_worldOwned.push_back(m);std::wstring p=lowerW(files[i]);uint32_t h=mapHash(p);float x=0,z=0,yaw=(h%6283)*.001f,yoff=0,target=1.7f;bool coll=false;XMFLOAT4 tint{1,1,1,1};
        bool road=p.find(L"road")!=std::wstring::npos||p.find(L"path")!=std::wstring::npos||p.find(L"tile")!=std::wstring::npos;
        bool water=p.find(L"river")!=std::wstring::npos||p.find(L"water")!=std::wstring::npos;
        bool building=p.find(L"building")!=std::wstring::npos||p.find(L"house")!=std::wstring::npos||p.find(L"tower")!=std::wstring::npos||p.find(L"castle")!=std::wstring::npos||p.find(L"wall")!=std::wstring::npos||p.find(L"gate")!=std::wstring::npos;
        bool tree=p.find(L"tree")!=std::wstring::npos;bool rock=p.find(L"rock")!=std::wstring::npos||p.find(L"boulder")!=std::wstring::npos;bool foliage=p.find(L"grass")!=std::wstring::npos||p.find(L"bush")!=std::wstring::npos||p.find(L"flower")!=std::wstring::npos;
        if(kind==1&&road&&!water&&!laneTile)laneTile=m;if(kind==1&&water&&!riverTile)riverTile=m;
        if(kind==1&&(road||water)){float ox,oz;if(decorSpot(h,i,ox,oz,2.4f)){addScene(m,ox,oz,yaw,road?3.0f:3.4f,false,tex,.01f);}continue;}
        if(!decorSpot(h,i,x,z,building?5.2f:(tree?3.8f:2.8f))){x=(i&1)?-92.f:92.f;z=-88.f+(float)(i%44)*4.f;}
        if(kind==0){target=tree?4.2f:(rock?2.6f:(foliage?1.15f:1.8f));coll=(tree||rock)&&((h&3u)!=0u);}
        else if(kind==1){target=building?4.7f:(rock?2.4f:1.8f);coll=building&&((h&1u)==0u);}
        else if(kind==2){target=building?4.0f:(p.find(L"crate")!=std::wstring::npos||p.find(L"barrel")!=std::wstring::npos?1.45f:2.0f);coll=building&&((h&3u)==0u);}
        else{target=1.15f;coll=false;}
        addScene(m,x,z,yaw,target,coll,tex,yoff,tint);
    }
    if(kind==1)buildLaneGeometry(laneTile,riverTile,tex);
}
static bool buildProfessionalMap(){g_scene.clear();g_mapOccupied.clear();for(auto* m:g_worldOwned){killMesh(*m);delete m;}g_worldOwned.clear();if(!g_forestSRV)loadTextureWIC(L"ExternalAssets\\KayKitForest\\Assets\\obj\\forest_texture.png",&g_forestSRV);if(!g_medievalSRV)loadTextureWIC(L"ExternalAssets\\KayKitMedieval\\Textures\\hexagons_medieval.png",&g_medievalSRV);if(!g_dungeonFullSRV)loadTextureWIC(L"ExternalAssets\\KayKitDungeonFull\\Assets\\textures\\dungeon_texture.png",&g_dungeonFullSRV);placeFullPack(L"ExternalAssets\\KayKitForest\\Assets\\obj",0,g_forestSRV);placeFullPack(L"ExternalAssets\\KayKitMedieval\\Assets\\obj",1,g_medievalSRV);placeFullPack(L"ExternalAssets\\KayKitDungeonFull\\Assets\\obj",2,g_dungeonFullSRV);placeFullPack(L"ExternalAssets\\KayKitAdventurersFull\\Assets\\obj",3,nullptr);return g_worldOwned.size()>400;}

static void resolveCharacter(Character& c){
'@
R $anchor $world 'constructor del mapa completo'

$old='auto add=[&](MeshGPU& m,float px,float pz,float yaw,float sc){g_scene.push_back({&m,px,pz,yaw,sc,true});}; for(int i=-4;i<=4;++i){add((i%3==0)?g_crates:((i&1)?g_barrel:g_rubble),i*16.f,24.f+(i&1)*12.f,i*.31f,1.25f);} for(int i=-4;i<=4;++i)add((i&1)?g_rubble:g_crates,i*18.f,-36.f-(i&1)*9.f,-i*.27f,1.2f);'
$new='if(!buildProfessionalMap()){MessageBoxW(g_hwnd,L"No pude construir el mapa completo con los cuatro packs KayKit.",L"SPVersion",MB_ICONERROR);return 4;}'
R $old $new 'reemplazo del escenario de prueba'

R 'if(g_gameState!=GameState::Playing){g_player.moving=false;poseCharacter(g_player,g_gameState==GameState::Defeated?"Death_A":"Idle_B",dt);g_attackQueued=false;return;}' 'if(g_gameState==GameState::Defeated){g_player.moving=false;poseCharacter(g_player,"Death_A",dt);g_attackQueued=false;if(g_player.animTime>.95f){g_gameState=GameState::Playing;g_player.health=1.f;g_damageFlash=0.f;g_player.pos={-68.f,groundH(-68.f,-68.f),-68.f};g_player.home=g_player.pos;g_player.activeClip.clear();g_player.animTime=0.f;g_camTarget={g_player.pos.x,g_player.pos.y+1.f,g_player.pos.z};}return;}if(g_gameState!=GameState::Playing){g_player.moving=false;poseCharacter(g_player,"Idle_B",dt);g_attackQueued=false;return;}' 'respawn automatico en base'
R 'if(g_gameState==GameState::Defeated){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.04f,.02f,.02f,.52f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("HAS MUERTO",cx-135.f,g_h*.5f-48.f,4.f,{.95f,.18f,.14f,1});drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});}' 'if(g_gameState==GameState::Defeated){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.04f,.02f,.02f,.42f});drawText5x7("HAS MUERTO",cx-135.f,g_h*.5f-18.f,4.f,{.95f,.18f,.14f,1});}' 'muerte sin boton de reinicio'

$spawnOld='g_player.pos={0,groundH(0,0),0};g_player.home=g_player.pos;g_npc.pos={8,groundH(8,9),9};g_npc.home=g_npc.pos;g_camTarget={0,1,0};'
$spawnNew='g_player.pos={-68.f,groundH(-68.f,-68.f),-68.f};g_player.home=g_player.pos;g_npc.pos={68.f,groundH(68.f,68.f),68.f};g_npc.home=g_npc.pos;g_camTarget={g_player.pos.x,g_player.pos.y+1.f,g_player.pos.z};'
if($code.Contains($spawnOld)){$code=$code.Replace($spawnOld,$spawnNew)}
else {$code=$code.Replace('g_player.pos={0,groundH(0,0),0};g_player.home=g_player.pos;', 'g_player.pos={-68.f,groundH(-68.f,-68.f),-68.f};g_player.home=g_player.pos;');$code=$code.Replace('g_npc.pos={8,groundH(8,9),9};g_npc.home=g_npc.pos;', 'g_npc.pos={68.f,groundH(68.f,68.f),68.f};g_npc.home=g_npc.pos;');$code=$code.Replace('g_camTarget={0,1,0};', 'g_camTarget={g_player.pos.x,g_player.pos.y+1.f,g_player.pos.z};')}
R 'killCharacter(g_npc);killCharacter(g_player);killMesh(g_playerShield);killMesh(g_npcSword);killMesh(g_playerSword);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);' 'killCharacter(g_npc);killCharacter(g_player);for(auto* m:g_worldOwned){killMesh(*m);delete m;}g_worldOwned.clear();g_mapOccupied.clear();rel(g_forestSRV);rel(g_medievalSRV);rel(g_dungeonFullSRV);killMesh(g_playerShield);killMesh(g_npcSword);killMesh(g_playerSword);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);' 'liberacion mapa'

if($code -eq $original){throw 'El parche del mundo no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa MOBA de tres rutas, rio, jungla y bases despejadas aplicado.'
