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
static void buildLaneRibbon(const std::vector<MapPt>&,int){}
'@
RR $laneOld $floorCode 'constructor de piso hexagonal calzado'

RR 'buildLaneRibbon(laneTop(),0);buildLaneRibbon(laneMid(),1);buildLaneRibbon(laneBot(),2);buildRiverRibbon();buildBridges();buildStrategicStructures();buildJungleWalls();buildBoundaryWalls();resetMobWave();' 'if(!buildHexFloor())return false;buildBridges();buildStrategicStructures();buildJungleWalls();buildBoundaryWalls();resetMobWave();' 'activar piso hexagonal en todo el mapa'

$renderOld='drawMeshWorld(g_terrain,XMMatrixIdentity(),{.92f,.92f,.92f,1},1,g_groundSRV); for(const auto& o:g_scene){float dx=o.x-g_camTarget.x,dz=o.z-g_camTarget.z;if(dx*dx+dz*dz<15000.f)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);}'
$renderNew='drawMeshWorld(g_terrain,XMMatrixIdentity(),{.92f,.92f,.92f,1},1,g_groundSRV);drawMeshWorld(g_hexFloorTextured,XMMatrixIdentity(),{1,1,1,1},1,g_groundSRV);drawMeshWorld(g_hexFloorTransit,XMMatrixIdentity(),{.58f,.54f,.46f,1},0,nullptr); for(const auto& o:g_scene){float dx=o.x-g_camTarget.x,dz=o.z-g_camTarget.z;if(dx*dx+dz*dz<15000.f)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);}'
RR $renderOld $renderNew 'render de piso hexagonal con textura de suelo y transito sin textura'

$cleanupOld='killMesh(g_terrain);rel(g_uiWhite);'
$cleanupNew='killMesh(g_hexFloorTransit);killMesh(g_hexFloorTextured);killMesh(g_terrain);rel(g_uiWhite);'
if($code.Contains($cleanupOld)){$code=$code.Replace($cleanupOld,$cleanupNew)}

if($code -eq $original){throw 'El parche v10 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Piso v10 aplicado: hexes de huella completa sin grietas, textura de suelo fuera de transito y hexes transitables sin textura.'
