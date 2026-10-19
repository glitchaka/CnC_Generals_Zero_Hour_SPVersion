param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function R([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

R 'struct SceneObj { MeshGPU* mesh=nullptr; float x=0,z=0,yaw=0,scale=1; bool collider=true; };' 'struct SceneObj { MeshGPU* mesh=nullptr; float x=0,z=0,yaw=0,scale=1; bool collider=true; ID3D11ShaderResourceView* tex=nullptr; XMFLOAT4 tint{1,1,1,1}; float yOffset=0; };' 'SceneObj extendido'
R 'static std::vector<SceneObj> g_scene;' 'static std::vector<SceneObj> g_scene; static std::vector<MeshGPU*> g_worldOwned; static ID3D11ShaderResourceView *g_forestSRV=nullptr,*g_medievalSRV=nullptr,*g_dungeonFullSRV=nullptr;' 'recursos del mapa'
R 'static XMMATRIX objectWorld(const SceneObj& o){ return XMMatrixScaling(o.scale,o.scale,o.scale)*XMMatrixRotationY(o.yaw)*XMMatrixTranslation(o.x,groundH(o.x,o.z),o.z); }' 'static XMMATRIX objectWorld(const SceneObj& o){ return XMMatrixScaling(o.scale,o.scale,o.scale)*XMMatrixRotationY(o.yaw)*XMMatrixTranslation(o.x,groundH(o.x,o.z)+o.yOffset,o.z); }' 'altura de props'
R 'for(const auto& o:g_scene)drawMeshWorld(*o.mesh,objectWorld(o),{1,1,1,1},1,g_kaySRV);' 'for(const auto& o:g_scene)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);' 'texturas de escena'

$anchor='static void resolveCharacter(Character& c){'
$world=@'
struct MapPt{float x,z;};
static std::wstring lowerW(std::wstring s){for(auto& c:s){if(c>=L'A'&&c<=L'Z')c=(wchar_t)(c-L'A'+L'a');}return s;}
static void collectObjFiles(const std::wstring& root,std::vector<std::wstring>& out){WIN32_FIND_DATAW fd{};HANDLE h=FindFirstFileW((root+L"\\*").c_str(),&fd);if(h==INVALID_HANDLE_VALUE)return;do{if(fd.cFileName[0]==L'.')continue;std::wstring p=root+L"\\"+fd.cFileName;if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)collectObjFiles(p,out);else{std::wstring n=lowerW(fd.cFileName);if(n.size()>4&&n.substr(n.size()-4)==L".obj")out.push_back(p);}}while(FindNextFileW(h,&fd));FindClose(h);}
static uint32_t mapHash(const std::wstring& s){uint32_t h=2166136261u;for(wchar_t c:s){h^=(uint32_t)c;h*=16777619u;}return h;}
static float meshScale(const MeshGPU& m,float target){float ex=m.bmax.x-m.bmin.x,ey=m.bmax.y-m.bmin.y,ez=m.bmax.z-m.bmin.z,e=(std::max)(.01f,(std::max)(ex,(std::max)(ey,ez)));return target/e;}
static float d2(float ax,float az,float bx,float bz){float x=ax-bx,z=az-bz;return x*x+z*z;}
static float pointSegDist(float x,float z,MapPt a,MapPt b){float vx=b.x-a.x,vz=b.z-a.z,wx=x-a.x,wz=z-a.z,den=vx*vx+vz*vz,t=den>1e-6f?clampf((wx*vx+wz*vz)/den,0.f,1.f):0.f;float px=a.x+t*vx,pz=a.z+t*vz;return sqrtf(d2(x,z,px,pz));}
static float polyDist(float x,float z,const std::vector<MapPt>& p){float d=1e9f;for(size_t i=1;i<p.size();++i)d=(std::min)(d,pointSegDist(x,z,p[i-1],p[i]));return d;}
static const std::vector<MapPt>& laneMid(){static const std::vector<MapPt> p={{-72,-72},{-46,-46},{-22,-22},{0,0},{22,22},{46,46},{72,72}};return p;}
static const std::vector<MapPt>& laneTop(){static const std::vector<MapPt> p={{-72,-72},{-72,-38},{-72,18},{-70,52},{-56,70},{-18,72},{38,72},{72,72}};return p;}
static const std::vector<MapPt>& laneBot(){static const std::vector<MapPt> p={{-72,-72},{-38,-72},{18,-72},{52,-70},{70,-56},{72,-18},{72,38},{72,72}};return p;}
static const std::vector<MapPt>& riverLine(){static const std::vector<MapPt> p={{-76,54},{-54,40},{-28,22},{0,0},{28,-22},{54,-40},{76,-54}};return p;}
static const std::vector<MapPt>& jungleA(){static const std::vector<MapPt> p={{-61,-52},{-50,-34},{-52,-8},{-61,18},{-67,43}};return p;}
static const std::vector<MapPt>& jungleB(){static const std::vector<MapPt> p={{-52,-61},{-34,-50},{-8,-52},{18,-61},{43,-67}};return p;}
static const std::vector<MapPt>& jungleC(){static const std::vector<MapPt> p={{61,52},{50,34},{52,8},{61,-18},{67,-43}};return p;}
static const std::vector<MapPt>& jungleD(){static const std::vector<MapPt> p={{52,61},{34,50},{8,52},{-18,61},{-43,67}};return p;}
static bool inBaseClear(float x,float z,float extra){return d2(x,z,-72.f,-72.f)<(13.f+extra)*(13.f+extra)||d2(x,z,72.f,72.f)<(13.f+extra)*(13.f+extra);}
static bool inObjectiveClear(float x,float z,float extra){return d2(x,z,-27.f,25.f)<(10.f+extra)*(10.f+extra)||d2(x,z,27.f,-25.f)<(10.f+extra)*(10.f+extra);}
static bool blocksGameplay(float x,float z,float r){float lane=10.5f+r,river=5.8f+r,path=4.2f+r;if(polyDist(x,z,laneMid())<lane||polyDist(x,z,laneTop())<lane||polyDist(x,z,laneBot())<lane)return true;if(polyDist(x,z,riverLine())<river)return true;if(polyDist(x,z,jungleA())<path||polyDist(x,z,jungleB())<path||polyDist(x,z,jungleC())<path||polyDist(x,z,jungleD())<path)return true;if(inBaseClear(x,z,r)||inObjectiveClear(x,z,r))return true;return false;}
static bool tooCloseToCollider(float x,float z,float r){for(const auto& o:g_scene){if(!o.collider||!o.mesh)continue;float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.36f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.36f,rr=(std::max)(.7f,(std::max)(rx,rz))+r;if(d2(x,z,o.x,o.z)<rr*rr)return true;}return false;}
static bool chooseJunglePosition(uint32_t h,float radius,float& x,float& z){for(int a=0;a<96;++a){uint32_t k=h+0x9e3779b9u*(a+1);float fx=((int)(k%14400)-7200)/100.f,fz=((int)((k/149)%14400)-7200)/100.f;if(fabsf(fx)>69.f||fabsf(fz)>69.f)continue;if(blocksGameplay(fx,fz,radius)||tooCloseToCollider(fx,fz,radius*.62f))continue;x=fx;z=fz;return true;}return false;}
static bool chooseBaseRing(uint32_t h,bool blue,float radius,float& x,float& z){float cx=blue?-72.f:72.f,cz=blue?-72.f:72.f;for(int a=0;a<64;++a){float ang=((h+a*977u)%6283)*.001f,rad=16.f+(float)((h/17+a*137)%1100)/100.f;float fx=cx+cosf(ang)*rad,fz=cz+sinf(ang)*rad;if(fabsf(fx)>84.f||fabsf(fz)>84.f)continue;if(blocksGameplay(fx,fz,radius)||tooCloseToCollider(fx,fz,radius*.65f))continue;x=fx;z=fz;return true;}return false;}
static bool chooseObjectiveRing(uint32_t h,bool upper,float radius,float& x,float& z){float cx=upper?-27.f:27.f,cz=upper?25.f:-25.f;for(int a=0;a<48;++a){float ang=((h+a*761u)%6283)*.001f,rad=12.f+(float)((h/29+a*101)%650)/100.f;float fx=cx+cosf(ang)*rad,fz=cz+sinf(ang)*rad;if(blocksGameplay(fx,fz,radius)||tooCloseToCollider(fx,fz,radius*.55f))continue;x=fx;z=fz;return true;}return false;}
static void addSceneMesh(MeshGPU* m,float x,float z,float yaw,float target,bool coll,ID3D11ShaderResourceView* tex,XMFLOAT4 tint={1,1,1,1},float yoff=0){float sc=meshScale(*m,target);g_scene.push_back({m,x,z,yaw,sc,coll,tex,tint,yoff});}
static void placeLaneSurface(MeshGPU* m,const std::vector<MapPt>& lane,size_t idx,ID3D11ShaderResourceView* tex){size_t seg=(idx%(lane.size()-1));MapPt a=lane[seg],b=lane[seg+1];float t=((idx/(lane.size()-1))%7+1)/8.f;float x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t,yaw=atan2f(b.x-a.x,b.z-a.z);addSceneMesh(m,x,z,yaw,5.0f,false,tex,{.94f,.94f,.94f,1},.03f);}
static void placeRiverSurface(MeshGPU* m,size_t idx,ID3D11ShaderResourceView* tex){const auto& l=riverLine();size_t seg=idx%(l.size()-1);MapPt a=l[seg],b=l[seg+1];float t=((idx/(l.size()-1))%6+1)/7.f;float x=a.x+(b.x-a.x)*t,z=a.z+(b.z-a.z)*t,yaw=atan2f(b.x-a.x,b.z-a.z);addSceneMesh(m,x,z,yaw,4.8f,false,tex,{.74f,.84f,.92f,1},-.04f);}
static void placeFullPack(const std::wstring& root,int kind,ID3D11ShaderResourceView* tex){std::vector<std::wstring> files;collectObjFiles(root,files);size_t roadN=0,riverN=0;for(size_t i=0;i<files.size();++i){MeshGPU* m=new MeshGPU;if(!loadOBJ(files[i],*m)){delete m;continue;}g_worldOwned.push_back(m);std::wstring p=lowerW(files[i]);uint32_t h=mapHash(p);float x=0,z=0,yaw=(h%6283)*.001f,target=1.8f,yoff=0;bool coll=true;XMFLOAT4 tint{1,1,1,1};
    bool road=p.find(L"road")!=std::wstring::npos||p.find(L"path")!=std::wstring::npos||p.find(L"tile")!=std::wstring::npos;
    bool river=p.find(L"river")!=std::wstring::npos||p.find(L"water")!=std::wstring::npos;
    bool building=p.find(L"building")!=std::wstring::npos||p.find(L"house")!=std::wstring::npos||p.find(L"tower")!=std::wstring::npos||p.find(L"castle")!=std::wstring::npos||p.find(L"wall")!=std::wstring::npos||p.find(L"gate")!=std::wstring::npos;
    bool vegetation=p.find(L"tree")!=std::wstring::npos||p.find(L"bush")!=std::wstring::npos||p.find(L"grass")!=std::wstring::npos||p.find(L"flower")!=std::wstring::npos;
    bool rock=p.find(L"rock")!=std::wstring::npos||p.find(L"stone")!=std::wstring::npos;
    if(kind==1&&road){const std::vector<MapPt>* lp=&laneMid();if((roadN%3)==1)lp=&laneTop();else if((roadN%3)==2)lp=&laneBot();placeLaneSurface(m,*lp,roadN++,tex);continue;}
    if(kind==1&&river){placeRiverSurface(m,riverN++,tex);continue;}
    if(kind==1&&building){bool blue=(h&1u)==0;target=(p.find(L"tower")!=std::wstring::npos)?5.2f:4.4f;float r=target*.62f;if(!chooseBaseRing(h,blue,r,x,z)){coll=false;chooseJunglePosition(h,0,x,z);}addSceneMesh(m,x,z,yaw,target,coll,tex,tint,yoff);continue;}
    if(kind==2){bool upper=(h&1u)==0;target=building?3.8f:(rock?2.5f:1.9f);float r=target*.58f;if(!chooseObjectiveRing(h,upper,r,x,z)&&!chooseJunglePosition(h,r,x,z)){coll=false;x=((int)(h%12000)-6000)/100.f;z=((int)((h/113)%12000)-6000)/100.f;}coll=coll&&p.find(L"decal")==std::wstring::npos;addSceneMesh(m,x,z,yaw,target,coll,tex,tint,yoff);continue;}
    if(kind==3){bool blue=(h&1u)==0;target=1.05f;coll=false;if(!chooseBaseRing(h,blue,.3f,x,z)){x=blue?-80.f:80.f;z=blue?-62.f:62.f;}addSceneMesh(m,x,z,yaw,target,false,tex,tint,yoff);continue;}
    target=vegetation?(p.find(L"tree")!=std::wstring::npos?4.2f:1.15f):(rock?2.15f:1.55f);coll=!vegetation||p.find(L"tree")!=std::wstring::npos;float r=coll?target*.52f:.2f;if(!chooseJunglePosition(h,r,x,z)){coll=false;for(int a=0;a<16;++a){float ang=((h+a*431u)%6283)*.001f,rad=76.f+(h%700)/100.f;x=cosf(ang)*rad;z=sinf(ang)*rad;if(!inBaseClear(x,z,2.f))break;}}addSceneMesh(m,x,z,yaw,target,coll,tex,tint,yoff);
}}
static bool buildProfessionalMap(){g_scene.clear();for(auto* m:g_worldOwned){killMesh(*m);delete m;}g_worldOwned.clear();if(!g_forestSRV)loadTextureWIC(L"ExternalAssets\\KayKitForest\\Assets\\obj\\forest_texture.png",&g_forestSRV);if(!g_medievalSRV)loadTextureWIC(L"ExternalAssets\\KayKitMedieval\\Textures\\hexagons_medieval.png",&g_medievalSRV);if(!g_dungeonFullSRV)loadTextureWIC(L"ExternalAssets\\KayKitDungeonFull\\Assets\\textures\\dungeon_texture.png",&g_dungeonFullSRV);placeFullPack(L"ExternalAssets\\KayKitForest\\Assets\\obj",0,g_forestSRV);placeFullPack(L"ExternalAssets\\KayKitMedieval\\Assets\\obj",1,g_medievalSRV);placeFullPack(L"ExternalAssets\\KayKitDungeonFull\\Assets\\obj",2,g_dungeonFullSRV);placeFullPack(L"ExternalAssets\\KayKitAdventurersFull\\Assets\\obj",3,nullptr);return g_worldOwned.size()>400;}

static void resolveCharacter(Character& c){
'@
R $anchor $world 'constructor del mapa MOBA'

$old='auto add=[&](MeshGPU& m,float px,float pz,float yaw,float sc){g_scene.push_back({&m,px,pz,yaw,sc,true});}; for(int i=-4;i<=4;++i){add((i%3==0)?g_crates:((i&1)?g_barrel:g_rubble),i*16.f,24.f+(i&1)*12.f,i*.31f,1.25f);} for(int i=-4;i<=4;++i)add((i&1)?g_rubble:g_crates,i*18.f,-36.f-(i&1)*9.f,-i*.27f,1.2f);'
$new='if(!buildProfessionalMap()){MessageBoxW(g_hwnd,L"No pude construir el mapa MOBA completo con los cuatro packs KayKit.",L"SPVersion",MB_ICONERROR);return 4;}'
R $old $new 'reemplazo del escenario de prueba'

R 'if(g_gameState!=GameState::Playing){g_player.moving=false;poseCharacter(g_player,g_gameState==GameState::Defeated?"Death_A":"Idle_B",dt);g_attackQueued=false;return;}' 'if(g_gameState==GameState::Defeated){g_player.moving=false;poseCharacter(g_player,"Death_A",dt);g_attackQueued=false;if(g_player.animTime>.95f){g_gameState=GameState::Playing;g_player.health=1.f;g_damageFlash=0.f;g_player.pos={-72.f,groundH(-72.f,-72.f),-72.f};g_player.home=g_player.pos;g_player.activeClip.clear();g_player.animTime=0.f;g_camTarget={g_player.pos.x,g_player.pos.y+1.f,g_player.pos.z};}return;}if(g_gameState!=GameState::Playing){g_player.moving=false;poseCharacter(g_player,"Idle_B",dt);g_attackQueued=false;return;}' 'respawn automatico en base'
R 'if(g_gameState==GameState::Defeated){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.04f,.02f,.02f,.52f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("HAS MUERTO",cx-135.f,g_h*.5f-48.f,4.f,{.95f,.18f,.14f,1});drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});}' 'if(g_gameState==GameState::Defeated){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.04f,.02f,.02f,.42f});drawText5x7("HAS MUERTO",cx-135.f,g_h*.5f-18.f,4.f,{.95f,.18f,.14f,1});}' 'muerte sin boton de reinicio'

$spawnOld='g_player.pos={0,groundH(0,0),0};g_player.home=g_player.pos;g_npc.pos={8,groundH(8,9),9};g_npc.home=g_npc.pos;g_camTarget={0,1,0};'
$spawnNew='g_player.pos={-72.f,groundH(-72.f,-72.f),-72.f};g_player.home=g_player.pos;g_npc.pos={72.f,groundH(72.f,72.f),72.f};g_npc.home=g_npc.pos;g_camTarget={g_player.pos.x,g_player.pos.y+1.f,g_player.pos.z};'
if($code.Contains($spawnOld)){$code=$code.Replace($spawnOld,$spawnNew)}
else {
  $code=$code.Replace('g_player.pos={0,groundH(0,0),0};g_player.home=g_player.pos;', 'g_player.pos={-72.f,groundH(-72.f,-72.f),-72.f};g_player.home=g_player.pos;')
  $code=$code.Replace('g_npc.pos={8,groundH(8,9),9};g_npc.home=g_npc.pos;', 'g_npc.pos={72.f,groundH(72.f,72.f),72.f};g_npc.home=g_npc.pos;')
  $code=$code.Replace('g_camTarget={0,1,0};', 'g_camTarget={g_player.pos.x,g_player.pos.y+1.f,g_player.pos.z};')
}
R 'killCharacter(g_npc);killCharacter(g_player);killMesh(g_playerShield);killMesh(g_npcSword);killMesh(g_playerSword);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);' 'killCharacter(g_npc);killCharacter(g_player);for(auto* m:g_worldOwned){killMesh(*m);delete m;}g_worldOwned.clear();rel(g_forestSRV);rel(g_medievalSRV);rel(g_dungeonFullSRV);killMesh(g_playerShield);killMesh(g_npcSword);killMesh(g_playerSword);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);' 'liberacion mapa'

if($code -eq $original){throw 'El parche del mundo no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa MOBA con tres carriles, rio, jungla y bases despejadas aplicado.'
