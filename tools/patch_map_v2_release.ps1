param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$start=$code.IndexOf('struct MapPt{float x,z;};')
$marker='static void resolveCharacter(Character& c){'
$end=$code.IndexOf($marker,$start)
if($start -lt 0 -or $end -lt 0){throw 'No encontre el bloque del mapa anterior.'}

$new=@'
struct MapPt{float x,z;};
static std::vector<MeshGPU*> g_laneTiles,g_riverTiles;
static MeshGPU *g_blueTowerMesh=nullptr,*g_redTowerMesh=nullptr,*g_blueCoreMesh=nullptr,*g_redCoreMesh=nullptr;
static std::wstring lowerW(std::wstring s){for(auto& c:s){if(c>=L'A'&&c<=L'Z')c=(wchar_t)(c-L'A'+L'a');}return s;}
static void collectObjFiles(const std::wstring& root,std::vector<std::wstring>& out){WIN32_FIND_DATAW fd{};HANDLE h=FindFirstFileW((root+L"\\*").c_str(),&fd);if(h==INVALID_HANDLE_VALUE)return;do{if(fd.cFileName[0]==L'.')continue;std::wstring p=root+L"\\"+fd.cFileName;if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)collectObjFiles(p,out);else{std::wstring n=lowerW(fd.cFileName);if(n.size()>4&&n.substr(n.size()-4)==L".obj")out.push_back(p);}}while(FindNextFileW(h,&fd));FindClose(h);}
static uint32_t mapHash(const std::wstring& s){uint32_t h=2166136261u;for(wchar_t c:s){h^=(uint32_t)c;h*=16777619u;}return h;}
static float meshScale(const MeshGPU& m,float target){float ex=m.bmax.x-m.bmin.x,ey=m.bmax.y-m.bmin.y,ez=m.bmax.z-m.bmin.z,e=(std::max)(.01f,(std::max)(ex,(std::max)(ey,ez)));return target/e;}
static float d2(float ax,float az,float bx,float bz){float x=ax-bx,z=az-bz;return x*x+z*z;}
static float pointSegDist(float x,float z,MapPt a,MapPt b){float vx=b.x-a.x,vz=b.z-a.z,wx=x-a.x,wz=z-a.z,den=vx*vx+vz*vz,t=den>1e-6f?clampf((wx*vx+wz*vz)/den,0.f,1.f):0.f;float px=a.x+t*vx,pz=a.z+t*vz;return sqrtf(d2(x,z,px,pz));}
static float polyDist(float x,float z,const std::vector<MapPt>& p){float d=1e9f;for(size_t i=1;i<p.size();++i)d=(std::min)(d,pointSegDist(x,z,p[i-1],p[i]));return d;}

// Diseno de tres carriles inspirado en la topologia de Summoner's Rift: bases diagonales,
// mid directo y top/bot rodeando las junglas. Las curvas estan separadas para que se lean
// como carriles distintos y no como una sola diagonal.
static const std::vector<MapPt>& laneMid(){static const std::vector<MapPt> p={{-92,-92},{-70,-70},{-46,-46},{-22,-22},{0,0},{22,22},{46,46},{70,70},{92,92}};return p;}
static const std::vector<MapPt>& laneTop(){static const std::vector<MapPt> p={{-92,-92},{-102,-66},{-104,-28},{-103,14},{-98,52},{-82,82},{-52,98},{-14,103},{30,104},{68,102},{92,92}};return p;}
static const std::vector<MapPt>& laneBot(){static const std::vector<MapPt> p={{-92,-92},{-66,-102},{-28,-104},{14,-103},{52,-98},{82,-82},{98,-52},{103,-14},{104,30},{102,68},{92,92}};return p;}
static const std::vector<MapPt>& riverLine(){static const std::vector<MapPt> p={{-108,70},{-82,56},{-55,39},{-28,20},{0,0},{28,-20},{55,-39},{82,-56},{108,-70}};return p;}
static const std::vector<MapPt>& jungleBlueTop(){static const std::vector<MapPt> p={{-82,-66},{-72,-45},{-72,-18},{-80,8},{-88,36},{-90,62}};return p;}
static const std::vector<MapPt>& jungleBlueBot(){static const std::vector<MapPt> p={{-66,-82},{-45,-72},{-18,-72},{8,-80},{36,-88},{62,-90}};return p;}
static const std::vector<MapPt>& jungleRedTop(){static const std::vector<MapPt> p={{82,66},{72,45},{72,18},{80,-8},{88,-36},{90,-62}};return p;}
static const std::vector<MapPt>& jungleRedBot(){static const std::vector<MapPt> p={{66,82},{45,72},{18,72},{-8,80},{-36,88},{-62,90}};return p;}
static const std::array<MapPt,8>& camps(){static const std::array<MapPt,8> p={MapPt{-68,-40},MapPt{-40,-68},MapPt{-67,18},MapPt{18,-67},MapPt{68,40},MapPt{40,68},MapPt{67,-18},MapPt{-18,67}};return p;}
static bool inBaseClear(float x,float z,float extra){return d2(x,z,-92.f,-92.f)<(18.f+extra)*(18.f+extra)||d2(x,z,92.f,92.f)<(18.f+extra)*(18.f+extra);}
static bool inObjectiveClear(float x,float z,float extra){return d2(x,z,-31.f,27.f)<(12.f+extra)*(12.f+extra)||d2(x,z,31.f,-27.f)<(12.f+extra)*(12.f+extra);}
static bool inLane(float x,float z,float extra){float w=8.5f+extra;return polyDist(x,z,laneMid())<w||polyDist(x,z,laneTop())<w||polyDist(x,z,laneBot())<w;}
static bool inRiver(float x,float z,float extra){return polyDist(x,z,riverLine())<5.7f+extra;}
static bool inJunglePath(float x,float z,float extra){float w=4.0f+extra;return polyDist(x,z,jungleBlueTop())<w||polyDist(x,z,jungleBlueBot())<w||polyDist(x,z,jungleRedTop())<w||polyDist(x,z,jungleRedBot())<w;}
static bool blocksGameplay(float x,float z,float r){return inLane(x,z,r)||inRiver(x,z,r)||inJunglePath(x,z,r)||inBaseClear(x,z,r)||inObjectiveClear(x,z,r);}
static float colliderRadius(const SceneObj& o){if(!o.mesh)return 0;float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.38f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.38f;return (std::max)(.45f,(std::max)(rx,rz));}
static bool tooCloseToCollider(float x,float z,float r){for(const auto& o:g_scene){if(!o.collider)continue;float rr=colliderRadius(o)+r;if(d2(x,z,o.x,o.z)<rr*rr)return true;}return false;}
static void addSceneMesh(MeshGPU* m,float x,float z,float yaw,float target,bool coll,ID3D11ShaderResourceView* tex,XMFLOAT4 tint={1,1,1,1},float yoff=0){if(!m)return;float sc=meshScale(*m,target);g_scene.push_back({m,x,z,yaw,sc,coll,tex,tint,yoff});}
static MapPt samplePolyline(const std::vector<MapPt>& p,float u,float& yaw){u=clampf(u,0,1);float total=0;for(size_t i=1;i<p.size();++i){float dx=p[i].x-p[i-1].x,dz=p[i].z-p[i-1].z;total+=sqrtf(dx*dx+dz*dz);}float want=u*total,acc=0;for(size_t i=1;i<p.size();++i){float dx=p[i].x-p[i-1].x,dz=p[i].z-p[i-1].z,len=sqrtf(dx*dx+dz*dz);if(acc+len>=want||i==p.size()-1){float t=len>0?(want-acc)/len:0;yaw=atan2f(dx,dz);return {p[i-1].x+dx*t,p[i-1].z+dz*t};}acc+=len;}yaw=0;return p.back();}
static MapPt offsetPerp(MapPt p,float yaw,float amount){return {p.x+cosf(yaw)*amount,p.z-sinf(yaw)*amount};}
static void scenicBelt(MeshGPU* m,uint32_t h,float target,ID3D11ShaderResourceView* tex){float ang=(h%6283)*.001f;float rad=126.f+(float)((h/17)%2400)/100.f;float x=cosf(ang)*rad,z=sinf(ang)*rad;addSceneMesh(m,x,z,-ang,target,false,tex,{.92f,.94f,.98f,1});}
static bool chooseJungleSlot(uint32_t h,float r,float& x,float& z){for(int a=0;a<96;++a){uint32_t k=h+0x9e3779b9u*(a+1);float fx=((int)(k%19400)-9700)/100.f,fz=((int)((k/149)%19400)-9700)/100.f;if(fabsf(fx)>93.f||fabsf(fz)>93.f)continue;if(blocksGameplay(fx,fz,r)||tooCloseToCollider(fx,fz,r*.85f))continue;x=fx;z=fz;return true;}return false;}
static bool chooseCampSlot(uint32_t h,float r,float& x,float& z){const auto& c=camps();for(int a=0;a<40;++a){MapPt cc=c[(h+a)%c.size()];float ang=((h+a*733u)%6283)*.001f,rad=3.5f+(float)((h/31+a*97)%450)/100.f;float fx=cc.x+cosf(ang)*rad,fz=cc.z+sinf(ang)*rad;if(blocksGameplay(fx,fz,r)||tooCloseToCollider(fx,fz,r*.72f))continue;x=fx;z=fz;return true;}return false;}
static bool chooseBasePerimeter(uint32_t h,bool blue,float r,float& x,float& z){float cx=blue?-92.f:92.f,cz=blue?-92.f:92.f;for(int a=0;a<64;++a){float ang=((h+a*919u)%6283)*.001f,rad=22.f+(float)((h/19+a*83)%850)/100.f;float fx=cx+cosf(ang)*rad,fz=cz+sinf(ang)*rad;if(fabsf(fx)>118.f||fabsf(fz)>118.f)continue;if(inLane(fx,fz,r)||tooCloseToCollider(fx,fz,r*.8f))continue;x=fx;z=fz;return true;}return false;}
static bool chooseObjectivePerimeter(uint32_t h,bool upper,float r,float& x,float& z){float cx=upper?-31.f:31.f,cz=upper?27.f:-27.f;for(int a=0;a<48;++a){float ang=((h+a*677u)%6283)*.001f,rad=14.f+(float)((h/23+a*101)%550)/100.f;float fx=cx+cosf(ang)*rad,fz=cz+sinf(ang)*rad;if(inLane(fx,fz,r)||inRiver(fx,fz,r*.3f)||tooCloseToCollider(fx,fz,r*.7f))continue;x=fx;z=fz;return true;}return false;}

static void rememberStrategicMesh(const std::wstring& p,MeshGPU* m){
    if(p.find(L"building_tower_a_blue.obj")!=std::wstring::npos||p.find(L"building_watchtower_blue.obj")!=std::wstring::npos)g_blueTowerMesh=m;
    if(p.find(L"building_tower_a_red.obj")!=std::wstring::npos||p.find(L"building_watchtower_red.obj")!=std::wstring::npos)g_redTowerMesh=m;
    if(p.find(L"building_castle_blue.obj")!=std::wstring::npos||p.find(L"building_townhall_blue.obj")!=std::wstring::npos)g_blueCoreMesh=m;
    if(p.find(L"building_castle_red.obj")!=std::wstring::npos||p.find(L"building_townhall_red.obj")!=std::wstring::npos)g_redCoreMesh=m;
}
static void placeOriginalAsset(MeshGPU* m,const std::wstring& p,uint32_t h,int kind,ID3D11ShaderResourceView* tex){
    bool road=p.find(L"road")!=std::wstring::npos||p.find(L"path")!=std::wstring::npos||p.find(L"tile")!=std::wstring::npos||p.find(L"bridge")!=std::wstring::npos;
    bool water=p.find(L"river")!=std::wstring::npos||p.find(L"water")!=std::wstring::npos;
    bool blue=p.find(L"_blue")!=std::wstring::npos||p.find(L"\\blue\\")!=std::wstring::npos;
    bool red=p.find(L"_red")!=std::wstring::npos||p.find(L"\\red\\")!=std::wstring::npos;
    bool building=p.find(L"building_")!=std::wstring::npos||p.find(L"castle")!=std::wstring::npos||p.find(L"tower")!=std::wstring::npos||p.find(L"wall")!=std::wstring::npos||p.find(L"gate")!=std::wstring::npos;
    bool tree=p.find(L"tree")!=std::wstring::npos;
    bool plant=p.find(L"bush")!=std::wstring::npos||p.find(L"grass")!=std::wstring::npos||p.find(L"flower")!=std::wstring::npos;
    bool cliff=p.find(L"hill")!=std::wstring::npos||p.find(L"cliff")!=std::wstring::npos;
    bool rock=p.find(L"rock")!=std::wstring::npos||p.find(L"stone")!=std::wstring::npos;
    float x=0,z=0,yaw=(h%6283)*.001f,target=1.2f;bool coll=false;
    if(kind==1){rememberStrategicMesh(p,m);if(road){g_laneTiles.push_back(m);scenicBelt(m,h,2.2f,tex);return;}if(water){g_riverTiles.push_back(m);scenicBelt(m,h,2.4f,tex);return;}if(building&&(blue||red)){target=p.find(L"tower")!=std::wstring::npos?4.3f:5.2f;if(chooseBasePerimeter(h,blue,target*.45f,x,z)){addSceneMesh(m,x,z,yaw,target,true,tex);return;}}scenicBelt(m,h,building?4.1f:1.8f,tex);return;}
    if(kind==0){target=cliff?5.5f:(tree?4.1f:(plant?1.1f:(rock?2.0f:1.5f)));coll=tree||cliff||rock;float r=coll?target*.42f:.15f;if(chooseJungleSlot(h,r,x,z)){addSceneMesh(m,x,z,yaw,target,coll,tex);return;}scenicBelt(m,h,target,tex);return;}
    if(kind==2){bool structural=building||rock||p.find(L"column")!=std::wstring::npos||p.find(L"barrier")!=std::wstring::npos;bool objective=p.find(L"wall")!=std::wstring::npos||p.find(L"column")!=std::wstring::npos||p.find(L"rocks")!=std::wstring::npos||p.find(L"torch")!=std::wstring::npos||p.find(L"chest")!=std::wstring::npos;target=structural?2.5f:1.0f;coll=structural;if(objective&&chooseObjectivePerimeter(h,(h&1u)==0,target*.38f,x,z)){addSceneMesh(m,x,z,yaw,target,coll,tex);return;}if(chooseCampSlot(h,target*.30f,x,z)){addSceneMesh(m,x,z,yaw,target,false,tex);return;}scenicBelt(m,h,target,tex);return;}
    // Adventurers: armerias decorativas de las dos bases, sin colision.
    bool team=(h&1u)==0;float cx=team?-92.f:92.f,cz=team?-92.f:92.f;float ang=((h/7)%6283)*.001f,rad=10.5f+(h%350)/100.f;x=cx+cosf(ang)*rad;z=cz+sinf(ang)*rad;target=1.0f;addSceneMesh(m,x,z,yaw,target,false,tex);
}
static void placeFullPack(const std::wstring& root,int kind,ID3D11ShaderResourceView* tex){std::vector<std::wstring> files;collectObjFiles(root,files);for(const auto& file:files){MeshGPU* m=new MeshGPU;if(!loadOBJ(file,*m)){delete m;continue;}g_worldOwned.push_back(m);std::wstring p=lowerW(file);placeOriginalAsset(m,p,mapHash(p),kind,tex);}}

static void buildLaneRibbon(const std::vector<MapPt>& lane,int laneIndex){if(g_laneTiles.empty())return;const int samples=46;for(int i=0;i<samples;++i){float yaw=0;MapPt p=samplePolyline(lane,(i+.5f)/samples,yaw);MeshGPU* m=g_laneTiles[(i+laneIndex*7)%g_laneTiles.size()];addSceneMesh(m,p.x,p.z,yaw,5.6f,false,g_medievalSRV,{.92f,.90f,.82f,1},.035f);}}
static void buildRiverRibbon(){if(g_riverTiles.empty())return;const int samples=34;for(int i=0;i<samples;++i){float yaw=0;MapPt p=samplePolyline(riverLine(),(i+.5f)/samples,yaw);MeshGPU* m=g_riverTiles[i%g_riverTiles.size()];addSceneMesh(m,p.x,p.z,yaw,5.4f,false,g_medievalSRV,{.68f,.82f,.96f,1},-.035f);}}
static void placeTowerOnLane(MeshGPU* m,const std::vector<MapPt>& lane,float u,bool blue){if(!m)return;float yaw=0;MapPt p=samplePolyline(lane,u,yaw);p=offsetPerp(p,yaw,blue?-5.2f:5.2f);addSceneMesh(m,p.x,p.z,yaw,4.6f,true,g_medievalSRV);}
static void buildStrategicStructures(){
    // Tres torres por carril y equipo, fuera del eje transitable.
    const std::vector<MapPt>* lanes[3]={&laneTop(),&laneMid(),&laneBot()};
    for(auto* l:lanes){placeTowerOnLane(g_blueTowerMesh,*l,.18f,true);placeTowerOnLane(g_blueTowerMesh,*l,.31f,true);placeTowerOnLane(g_blueTowerMesh,*l,.43f,true);placeTowerOnLane(g_redTowerMesh,*l,.57f,false);placeTowerOnLane(g_redTowerMesh,*l,.69f,false);placeTowerOnLane(g_redTowerMesh,*l,.82f,false);}
    // Nucleo visual detras de la zona de aparicion, no encima del spawn.
    addSceneMesh(g_blueCoreMesh,-106.f,-106.f,.78f,7.0f,true,g_medievalSRV);
    addSceneMesh(g_redCoreMesh,106.f,106.f,-2.35f,7.0f,true,g_medievalSRV);
}
static void buildJungleWalls(){
    // Replicar arboles/rocas ya cargados como bordes visuales sin cerrar accesos.
    std::vector<MeshGPU*> solid;for(const auto& o:g_scene)if(o.mesh&&o.collider&&o.tex==g_forestSRV)solid.push_back(o.mesh);if(solid.empty())return;
    const std::vector<MapPt>* paths[4]={&jungleBlueTop(),&jungleBlueBot(),&jungleRedTop(),&jungleRedBot()};int n=0;for(auto* path:paths){for(int i=0;i<18;++i){float yaw=0;MapPt p=samplePolyline(*path,(i+.5f)/18.f,yaw);float side=(i&1)?7.2f:-7.2f;MapPt q=offsetPerp(p,yaw,side);if(inLane(q.x,q.z,1.3f)||inRiver(q.x,q.z,1.0f))continue;MeshGPU* m=solid[(n++)%solid.size()];addSceneMesh(m,q.x,q.z,yaw,2.8f,true,g_forestSRV);}}}
static bool buildProfessionalMap(){
    g_scene.clear();for(auto* m:g_worldOwned){killMesh(*m);delete m;}g_worldOwned.clear();g_laneTiles.clear();g_riverTiles.clear();g_blueTowerMesh=g_redTowerMesh=g_blueCoreMesh=g_redCoreMesh=nullptr;
    if(!g_forestSRV)loadTextureWIC(L"ExternalAssets\\KayKitForest\\Assets\\obj\\forest_texture.png",&g_forestSRV);
    if(!g_medievalSRV)loadTextureWIC(L"ExternalAssets\\KayKitMedieval\\Textures\\hexagons_medieval.png",&g_medievalSRV);
    if(!g_dungeonFullSRV)loadTextureWIC(L"ExternalAssets\\KayKitDungeonFull\\Assets\\textures\\dungeon_texture.png",&g_dungeonFullSRV);
    placeFullPack(L"ExternalAssets\\KayKitForest\\Assets\\obj",0,g_forestSRV);
    placeFullPack(L"ExternalAssets\\KayKitMedieval\\Assets\\obj",1,g_medievalSRV);
    placeFullPack(L"ExternalAssets\\KayKitDungeonFull\\Assets\\obj",2,g_dungeonFullSRV);
    placeFullPack(L"ExternalAssets\\KayKitAdventurersFull\\Assets\\obj",3,nullptr);
    buildLaneRibbon(laneTop(),0);buildLaneRibbon(laneMid(),1);buildLaneRibbon(laneBot(),2);buildRiverRibbon();buildStrategicStructures();buildJungleWalls();
    return g_worldOwned.size()>400&&g_laneTiles.size()>0;
}

static void resolveCharacter(Character& c){
'@

$code=$code.Substring(0,$start)+$new+$code.Substring($end+$marker.Length)
$code=$code.Replace('float dist=18.5f*g_zoom','float dist=22.5f*g_zoom')
$code=$code.Replace('static float g_camYaw=.78f,g_camPitch=.58f,g_zoom=1.f;','static float g_camYaw=.78f,g_camPitch=.72f,g_zoom=1.f;')
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa MOBA v2 aplicado: tres carriles despejados, jungla, rio, bases y assets fuera de las vias.'
