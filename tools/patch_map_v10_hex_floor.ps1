param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

function RR([string]$a,[string]$b,[string]$n){
  if(-not $script:code.Contains($a)){throw "No encontre: $n"}
  $script:code=$script:code.Replace($a,$b)
}

RR 'static std::vector<SceneObj> g_scene; static std::vector<MeshGPU*> g_worldOwned; static ID3D11ShaderResourceView *g_forestSRV=nullptr,*g_medievalSRV=nullptr,*g_dungeonFullSRV=nullptr;' 'static std::vector<SceneObj> g_scene; static std::vector<MeshGPU*> g_worldOwned; static ID3D11ShaderResourceView *g_forestSRV=nullptr,*g_medievalSRV=nullptr,*g_dungeonFullSRV=nullptr; static MeshGPU g_hexFloorTextured,g_hexFloorTransit;' 'meshes combinados del piso hexagonal'

$laneOld=@'
static void buildLaneRibbon(const std::vector<MapPt>& lane,int laneIndex){
    // Los carriles son superficies continuas de terreno, no una fila de piezas/rombos del atlas.
    XMFLOAT4 tint=laneIndex==1?XMFLOAT4{.74f,.70f,.61f,1}:XMFLOAT4{.70f,.67f,.58f,1};buildGroundRibbon(lane,7.0f,tint,.045f);
}
'@
$floorCode=@'
static bool loadHexTopCPU(const std::wstring& path,std::vector<Vertex>& outV,std::vector<uint32_t>& outI){
    std::ifstream f(path.c_str());if(!f)return false;std::vector<XMFLOAT3> p,n;std::vector<XMFLOAT2> uv;std::unordered_map<std::string,uint32_t> map;std::string line;
    auto emit=[&](const std::string& s)->uint32_t{auto it=map.find(s);if(it!=map.end())return it->second;int pi=0,ti=0,ni=0;sscanf_s(s.c_str(),"%d/%d/%d",&pi,&ti,&ni);Vertex a{};if(pi>0&&pi<=(int)p.size())a.p=p[pi-1];if(ni>0&&ni<=(int)n.size())a.n=n[ni-1];else a.n={0,1,0};if(ti>0&&ti<=(int)uv.size())a.uv=uv[ti-1];uint32_t id=(uint32_t)outV.size();outV.push_back(a);map[s]=id;return id;};
    auto topVertex=[&](const std::string& s)->bool{int pi=0;if(sscanf_s(s.c_str(),"%d",&pi)!=1||pi<=0||pi>(int)p.size())return false;return p[pi-1].y>-.10f;};
    while(std::getline(f,line)){std::istringstream ss(line);std::string t;ss>>t;if(t=="v"){XMFLOAT3 a;ss>>a.x>>a.y>>a.z;p.push_back(a);}else if(t=="vn"){XMFLOAT3 a;ss>>a.x>>a.y>>a.z;n.push_back(a);}else if(t=="vt"){XMFLOAT2 a;ss>>a.x>>a.y;uv.push_back(a);}else if(t=="f"){std::vector<std::string> face;std::string q;while(ss>>q)face.push_back(q);bool top=face.size()>=3;for(const auto& e:face)top=top&&topVertex(e);if(!top)continue;for(size_t k=1;k+1<face.size();++k){outI.push_back(emit(face[0]));outI.push_back(emit(face[k]));outI.push_back(emit(face[k+1]));}}}
    return !outV.empty()&&!outI.empty();
}
static void appendHexTile(const std::vector<Vertex>& srcV,const std::vector<uint32_t>& srcI,float cx,float cz,float scale,std::vector<Vertex>& dstV,std::vector<uint32_t>& dstI){
    uint32_t base=(uint32_t)dstV.size();dstV.reserve(dstV.size()+srcV.size());dstI.reserve(dstI.size()+srcI.size());
    for(const auto& sv:srcV){Vertex v=sv;v.p.x=cx+sv.p.x*scale;v.p.z=cz+sv.p.z*scale;v.p.y=groundH(v.p.x,v.p.z)+sv.p.y*scale+.095f;dstV.push_back(v);}for(uint32_t i:srcI)dstI.push_back(base+i);
}
static bool buildHexFloor(){
    killMesh(g_hexFloorTextured);killMesh(g_hexFloorTransit);
    std::vector<Vertex> srcV;std::vector<uint32_t> srcI;if(!loadHexTopCPU(L"ExternalAssets\\KayKitMedieval\\Assets\\obj\\tiles\\base\\hex_grass.obj",srcV,srcI))return false;
    std::vector<Vertex> texV,plainV;std::vector<uint32_t> texI,plainI;
    // El OBJ es pointy-top: ancho 2 y alto 2.309401. Este es el paso exacto de teselacion.
    // Solo copiamos la tapa superior; los laterales profundos del hex_grass no son suelo.
    const float S=1.55f;const float W=2.0f*S;const float H=2.309401f*S;const float ROW=1.732051f*S;const float COVER=170.f;
    int rMin=(int)floorf((-COVER-H)/ROW)-2,rMax=(int)ceilf((COVER+H)/ROW)+2;
    int cMin=(int)floorf((-COVER-W)/W)-3,cMax=(int)ceilf((COVER+W)/W)+3;
    for(int r=rMin;r<=rMax;++r){float cz=r*ROW;float xoff=(r&1)?W*.5f:0.f;for(int c=cMin;c<=cMax;++c){float cx=c*W+xoff;if(cx<-COVER-W||cx>COVER+W||cz<-COVER-H||cz>COVER+H)continue;bool transit=blocksGameplay(cx,cz,S*.65f);if(transit)appendHexTile(srcV,srcI,cx,cz,S,plainV,plainI);else appendHexTile(srcV,srcI,cx,cz,S,texV,texI);}}
    if(texV.empty()||plainV.empty())return false;bool a=makeMesh(texV,texI,g_hexFloorTextured,false);bool b=makeMesh(plainV,plainI,g_hexFloorTransit,false);return a&&b;
}
static void buildLaneRibbon(const std::vector<MapPt>&,int){}
'@
RR $laneOld $floorCode 'constructor de piso hexagonal calzado'

RR 'buildLaneRibbon(laneTop(),0);buildLaneRibbon(laneMid(),1);buildLaneRibbon(laneBot(),2);buildRiverRibbon();buildBridges();buildStrategicStructures();buildJungleWalls();buildBoundaryWalls();resetMobWave();' 'if(!buildHexFloor())return false;buildBridges();buildStrategicStructures();buildJungleWalls();buildBoundaryWalls();resetMobWave();' 'activar piso hexagonal en todo el mapa'

$renderOld='drawMeshWorld(g_terrain,XMMatrixIdentity(),{.92f,.92f,.92f,1},1,g_groundSRV); for(const auto& o:g_scene){float dx=o.x-g_camTarget.x,dz=o.z-g_camTarget.z;if(dx*dx+dz*dz<15000.f)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);}'
$renderNew='drawMeshWorld(g_terrain,XMMatrixIdentity(),{.92f,.92f,.92f,1},1,g_groundSRV);drawMeshWorld(g_hexFloorTextured,XMMatrixIdentity(),{1,1,1,1},1,g_medievalSRV);drawMeshWorld(g_hexFloorTransit,XMMatrixIdentity(),{.58f,.54f,.46f,1},0,nullptr); for(const auto& o:g_scene){float dx=o.x-g_camTarget.x,dz=o.z-g_camTarget.z;if(dx*dx+dz*dz<15000.f)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);}'
RR $renderOld $renderNew 'render de piso hexagonal con textura KayKit y transito sin textura'

$cleanupOld='killMesh(g_terrain);rel(g_uiWhite);'
$cleanupNew='killMesh(g_hexFloorTransit);killMesh(g_hexFloorTextured);killMesh(g_terrain);rel(g_uiWhite);'
if($code.Contains($cleanupOld)){$code=$code.Replace($cleanupOld,$cleanupNew)}

if($code -eq $original){throw 'El parche v10 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Piso v10 aplicado: tapas hexagonales calzadas, textura medieval correcta, transito sin textura y sin bloques profundos.'
