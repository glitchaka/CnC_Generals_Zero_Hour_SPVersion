param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

# 1-2) PISO: usar la geometria REAL de hex_grass.obj y calcular la reticula desde su huella real.
# Todas las piezas usan la misma geometria. Solo cambia el material: suelo texturizado o transito sin textura.
$oldFloor=@'
static void appendHexCell(float cx,float cz,float S,std::vector<Vertex>& dstV,std::vector<uint32_t>& dstI){
    // Huella exterior real de hex_grass.obj: x +/-1.0, z +/-1.154700.
    // Se genera la tapa con ESA huella completa, no con el anillo superior inset de +/-0.95,
    // porque ese inset era precisamente lo que dejaba grietas entre piezas.
    static const float hx[6]={0.f,1.f,1.f,0.f,-1.f,-1.f};
    static const float hz[6]={-1.154700f,-.577350f,.577350f,1.154700f,.577350f,-.577350f};
    uint32_t base=(uint32_t)dstV.size();
    Vertex c{};c.p={cx,groundH(cx,cz)+.090f,cz};c.n={0,1,0};c.uv={cx*.045f,cz*.045f};dstV.push_back(c);
    for(int i=0;i<6;++i){float x=cx+hx[i]*S,z=cz+hz[i]*S;Vertex v{};v.p={x,groundH(x,z)+.090f,z};v.n={0,1,0};v.uv={x*.045f,z*.045f};dstV.push_back(v);}
    for(uint32_t i=0;i<6;++i){dstI.push_back(base);dstI.push_back(base+1+i);dstI.push_back(base+1+((i+1)%6));}
}
static bool buildHexFloor(){
    killMesh(g_hexFloorTextured);killMesh(g_hexFloorTransit);
    std::vector<Vertex> texV,plainV;std::vector<uint32_t> texI,plainI;
    // Pointy-top exacto: ancho 2*S, paso vertical 1.5*R = 1.7320508*S,
    // filas alternas corridas medio ancho. Esta malla tesela sin huecos.
    const float S=1.55f;const float W=2.0f*S;const float H=2.309401f*S;const float ROW=1.7320508f*S;const float COVER=190.f;
    int rMin=(int)floorf((-COVER-H)/ROW)-2,rMax=(int)ceilf((COVER+H)/ROW)+2;
    int cMin=(int)floorf((-COVER-W)/W)-3,cMax=(int)ceilf((COVER+W)/W)+3;
    for(int r=rMin;r<=rMax;++r){
        float cz=r*ROW;float xoff=(r&1)?W*.5f:0.f;
        for(int c=cMin;c<=cMax;++c){
            float cx=c*W+xoff;if(cx<-COVER-W||cx>COVER+W||cz<-COVER-H||cz>COVER+H)continue;
            // Si cualquier parte del hex puede invadir carril/rio/camino/base/objetivo, queda sin textura.
            // 1.16*S cubre el radio exterior completo de la pieza.
            bool transit=blocksGameplay(cx,cz,S*1.16f);
            if(transit)appendHexCell(cx,cz,S,plainV,plainI);else appendHexCell(cx,cz,S,texV,texI);
        }
    }
    if(texV.empty()||plainV.empty())return false;
    bool a=makeMesh(texV,texI,g_hexFloorTextured,false);bool b=makeMesh(plainV,plainI,g_hexFloorTransit,false);return a&&b;
}
'@
$newFloor=@'
static bool loadFloorTileCPU(const std::wstring& path,std::vector<Vertex>& outV,std::vector<uint32_t>& outI){
    std::ifstream f(path.c_str());if(!f)return false;
    std::vector<XMFLOAT3> p,n;std::vector<XMFLOAT2> uv;std::unordered_map<std::string,uint32_t> map;std::string line;
    auto emit=[&](const std::string& s)->uint32_t{
        auto it=map.find(s);if(it!=map.end())return it->second;
        int pi=0,ti=0,ni=0;sscanf_s(s.c_str(),"%d/%d/%d",&pi,&ti,&ni);
        Vertex a{};if(pi>0&&pi<=(int)p.size())a.p=p[pi-1];if(ni>0&&ni<=(int)n.size())a.n=n[ni-1];else a.n={0,1,0};if(ti>0&&ti<=(int)uv.size())a.uv=uv[ti-1];
        uint32_t id=(uint32_t)outV.size();outV.push_back(a);map[s]=id;return id;
    };
    while(std::getline(f,line)){
        std::istringstream ss(line);std::string t;ss>>t;
        if(t=="v"){XMFLOAT3 a;ss>>a.x>>a.y>>a.z;p.push_back(a);}
        else if(t=="vn"){XMFLOAT3 a;ss>>a.x>>a.y>>a.z;n.push_back(a);}
        else if(t=="vt"){XMFLOAT2 a;ss>>a.x>>a.y;uv.push_back(a);}
        else if(t=="f"){std::vector<std::string> face;std::string q;while(ss>>q)face.push_back(q);for(size_t k=1;k+1<face.size();++k){outI.push_back(emit(face[0]));outI.push_back(emit(face[k]));outI.push_back(emit(face[k+1]));}}
    }
    return !outV.empty()&&!outI.empty();
}
static void appendFloorTile(const std::vector<Vertex>& srcV,const std::vector<uint32_t>& srcI,float cx,float cz,float S,std::vector<Vertex>& dstV,std::vector<uint32_t>& dstI){
    uint32_t base=(uint32_t)dstV.size();dstV.reserve(dstV.size()+srcV.size());dstI.reserve(dstI.size()+srcI.size());
    for(const auto& sv:srcV){Vertex v=sv;float x=cx+sv.p.x*S,z=cz+sv.p.z*S;v.p={x,groundH(x,z)+sv.p.y*S+.090f,z};v.uv={x*.045f,z*.045f};dstV.push_back(v);}for(uint32_t i:srcI)dstI.push_back(base+i);
}
static bool buildHexFloor(){
    killMesh(g_hexFloorTextured);killMesh(g_hexFloorTransit);
    std::vector<Vertex> tileV;std::vector<uint32_t> tileI;
    if(!loadFloorTileCPU(L"ExternalAssets\\KayKitMedieval\\Assets\\obj\\tiles\\base\\hex_grass.obj",tileV,tileI))return false;
    float minX=FLT_MAX,maxX=-FLT_MAX,minZ=FLT_MAX,maxZ=-FLT_MAX;
    for(const auto& v:tileV){minX=(std::min)(minX,v.p.x);maxX=(std::max)(maxX,v.p.x);minZ=(std::min)(minZ,v.p.z);maxZ=(std::max)(maxZ,v.p.z);}
    std::vector<Vertex> texV,plainV;std::vector<uint32_t> texI,plainI;
    const float S=1.55f;const float tileW=(maxX-minX)*S;const float tileH=(maxZ-minZ)*S;const float row=tileH*.75f;const float COVER=190.f;const float radius=(std::max)(tileW*.5f,tileH*.5f);
    int rMin=(int)floorf((-COVER-tileH)/row)-2,rMax=(int)ceilf((COVER+tileH)/row)+2;
    int cMin=(int)floorf((-COVER-tileW)/tileW)-3,cMax=(int)ceilf((COVER+tileW)/tileW)+3;
    for(int r=rMin;r<=rMax;++r){float cz=r*row;float xoff=(r&1)?tileW*.5f:0.f;for(int c=cMin;c<=cMax;++c){float cx=c*tileW+xoff;if(cx<-COVER-tileW||cx>COVER+tileW||cz<-COVER-tileH||cz>COVER+tileH)continue;bool transit=blocksGameplay(cx,cz,radius);if(transit)appendFloorTile(tileV,tileI,cx,cz,S,plainV,plainI);else appendFloorTile(tileV,tileI,cx,cz,S,texV,texI);}}
    if(texV.empty()||plainV.empty())return false;bool a=makeMesh(texV,texI,g_hexFloorTextured,false);bool b=makeMesh(plainV,plainI,g_hexFloorTransit,false);return a&&b;
}
'@
RR $oldFloor $newFloor 'piso con geometria original del rombo'

# El suelo visible es la reticula de piezas. No dejar la lamina continua antigua encima/debajo confundiendo el resultado.
RR 'drawMeshWorld(g_terrain,XMMatrixIdentity(),{.92f,.92f,.92f,1},1,g_groundSRV);drawMeshWorld(g_hexFloorTextured,XMMatrixIdentity(),{1,1,1,1},1,g_groundSRV);drawMeshWorld(g_hexFloorTransit,XMMatrixIdentity(),{.58f,.54f,.46f,1},0,nullptr);' 'drawMeshWorld(g_hexFloorTextured,XMMatrixIdentity(),{1,1,1,1},1,g_groundSRV);drawMeshWorld(g_hexFloorTransit,XMMatrixIdentity(),{.58f,.54f,.46f,1},0,nullptr);' 'quitar lamina continua de suelo del render'

# 3) CONSTRUCCIONES: escala natural del propio modelo (1.0). No se inventan alturas objetivo.
$oldAdd='static void addSceneMesh(MeshGPU* m,float x,float z,float yaw,float target,bool coll,ID3D11ShaderResourceView* tex,XMFLOAT4 tint={1,1,1,1},float yoff=0){if(!m)return;float sc=meshScale(*m,target);g_scene.push_back({m,x,z,yaw,sc,coll,tex,tint,yoff});}'
$newAdd=$oldAdd+' static void addSceneMeshNatural(MeshGPU* m,float x,float z,float yaw,bool coll,ID3D11ShaderResourceView* tex,XMFLOAT4 tint={1,1,1,1},float yoff=0){if(!m)return;g_scene.push_back({m,x,z,yaw,1.0f,coll,tex,tint,yoff});}'
RR $oldAdd $newAdd 'helper de escala natural'

RR 'if(building&&(blue||red)){target=p.find(L"tower")!=std::wstring::npos?12.0f:16.0f;if(chooseBasePerimeter(h,blue,target*.45f,x,z)){addSceneMesh(m,x,z,yaw,target,true,tex);return;}}' 'if(building&&(blue||red)){float naturalR=(std::max)(1.0f,(std::max)(m->bmax.x-m->bmin.x,m->bmax.z-m->bmin.z)*.38f);if(chooseBasePerimeter(h,blue,naturalR,x,z)){addSceneMeshNatural(m,x,z,yaw,true,tex);return;}}' 'edificios de base a escala natural'
RR 'static void placeTowerOnLane(MeshGPU* m,const std::vector<MapPt>& lane,float u,bool blue){if(!m)return;float yaw=0;MapPt p=samplePolyline(lane,u,yaw);p=offsetPerp(p,yaw,blue?-5.2f:5.2f);addSceneMesh(m,p.x,p.z,yaw,12.0f,true,g_medievalSRV);}' 'static void placeTowerOnLane(MeshGPU* m,const std::vector<MapPt>& lane,float u,bool blue){if(!m)return;float yaw=0;MapPt p=samplePolyline(lane,u,yaw);p=offsetPerp(p,yaw,blue?-5.2f:5.2f);addSceneMeshNatural(m,p.x,p.z,yaw,true,g_medievalSRV);}' 'torres a escala natural'
RR 'addSceneMesh(g_blueCoreMesh,-106.f,-106.f,.78f,20.0f,true,g_medievalSRV);' 'addSceneMeshNatural(g_blueCoreMesh,-106.f,-106.f,.78f,true,g_medievalSRV);' 'nucleo azul a escala natural'
RR 'addSceneMesh(g_redCoreMesh,106.f,106.f,-2.35f,20.0f,true,g_medievalSRV);' 'addSceneMeshNatural(g_redCoreMesh,106.f,106.f,-2.35f,true,g_medievalSRV);' 'nucleo rojo a escala natural'

# Dungeon/arquitectura estructural: tambien conserva la escala propia del asset.
$structOld='target=structural?2.5f:1.0f;coll=structural;if(objective&&chooseObjectivePerimeter(h,(h&1u)==0,target*.38f,x,z)){addSceneMesh(m,x,z,yaw,target,coll,tex);return;}if(chooseCampSlot(h,target*.30f,x,z)){addSceneMesh(m,x,z,yaw,target,false,tex);return;}'
$structNew='target=structural?2.5f:1.0f;coll=structural;if(objective&&chooseObjectivePerimeter(h,(h&1u)==0,structural?(std::max)(1.0f,(std::max)(m->bmax.x-m->bmin.x,m->bmax.z-m->bmin.z)*.38f):target*.38f,x,z)){if(structural)addSceneMeshNatural(m,x,z,yaw,coll,tex);else addSceneMesh(m,x,z,yaw,target,coll,tex);return;}if(chooseCampSlot(h,target*.30f,x,z)){if(structural)addSceneMeshNatural(m,x,z,yaw,false,tex);else addSceneMesh(m,x,z,yaw,target,false,tex);return;}'
RR $structOld $structNew 'arquitectura dungeon a escala natural'

# 4) VICTORIA: solo quitar cualquier peticion/tecla de reinicio. No se cambia el flujo de victoria.
$code=$code.Replace('drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});','')
$code=$code.Replace('if(g_gameState==GameState::Victory&&keyPressed(''R''))resetGame();','')
$code=$code.Replace('if(g_gameState==GameState::Victory && keyPressed(''R'')) resetGame();','')

if($code -eq $original){throw 'El parche v12 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v12 aplicado: rombos originales calzados por su huella real, textura de suelo por UV mundial, transito sin textura, construcciones a escala natural y sin reinicio al ganar.'
