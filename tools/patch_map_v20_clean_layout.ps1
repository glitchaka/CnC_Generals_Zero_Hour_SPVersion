param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

# Colecciones de mallas: cargar assets completos, pero no colocarlos durante el escaneo.
$anchor='static MeshGPU *g_blueTowerMesh=nullptr,*g_redTowerMesh=nullptr,*g_blueCoreMesh=nullptr,*g_redCoreMesh=nullptr,*g_boundaryWallMesh=nullptr;'
$insert=$anchor+' static std::vector<MeshGPU*> g_blueBaseBuildings,g_redBaseBuildings,g_forestCurated;'
if(-not $code.Contains($anchor)){throw 'No encontre globals estrategicos.'}
$code=$code.Replace($anchor,$insert)

$placePattern='(?s)static void placeOriginalAsset\(MeshGPU\* m,const std::wstring& p,uint32_t h,int kind,ID3D11ShaderResourceView\* tex\)\{.*?\n\}(?=\s*static bool runtimeAssetWanted)'
if(-not [regex]::IsMatch($code,$placePattern)){throw 'No encontre placeOriginalAsset.'}
$place=@'
static void placeOriginalAsset(MeshGPU* m,const std::wstring& p,uint32_t h,int kind,ID3D11ShaderResourceView* tex){
    if(!m)return;
    if(kind==1){
        rememberStrategicMesh(p,m);
        if(p.find(L"building_bridge_")!=std::wstring::npos){g_bridgeTiles.push_back(m);return;}
        if(p.find(L"\\tiles\\roads\\")!=std::wstring::npos||p.find(L"hex_road_")!=std::wstring::npos){g_laneTiles.push_back(m);return;}
        if(p.find(L"\\tiles\\rivers\\")!=std::wstring::npos||p.find(L"hex_water")!=std::wstring::npos){g_riverTiles.push_back(m);return;}
        bool blue=p.find(L"blue")!=std::wstring::npos,red=p.find(L"red")!=std::wstring::npos;
        bool support=p.find(L"building_barracks_")!=std::wstring::npos||p.find(L"building_blacksmith_")!=std::wstring::npos||p.find(L"building_archeryrange_")!=std::wstring::npos||p.find(L"building_church_")!=std::wstring::npos;
        if(support&&blue&&g_blueBaseBuildings.size()<4)g_blueBaseBuildings.push_back(m);
        if(support&&red&&g_redBaseBuildings.size()<4)g_redBaseBuildings.push_back(m);
        return;
    }
    if(kind==0){
        bool useful=p.find(L"tree")!=std::wstring::npos||p.find(L"rock")!=std::wstring::npos||p.find(L"stone")!=std::wstring::npos||p.find(L"cliff")!=std::wstring::npos;
        if(useful&&g_forestCurated.size()<10)g_forestCurated.push_back(m);
        return;
    }
    if(kind==2){
        if(!g_boundaryWallMesh&&(p.find(L"\\wall.obj")!=std::wstring::npos||p.find(L"wall_arched.obj")!=std::wstring::npos||p.find(L"wall_half.obj")!=std::wstring::npos))g_boundaryWallMesh=m;
        return;
    }
}
'@
$code=[regex]::Replace($code,$placePattern,$place,1)

# Bases y torres en posiciones deliberadas, usando escala nativa de los modelos.
$stratPattern='(?s)static void buildStrategicStructures\(\)\{.*?\n\}(?=\s*static void buildJungleWalls)'
if(-not [regex]::IsMatch($code,$stratPattern)){throw 'No encontre buildStrategicStructures.'}
$strat=@'
static void buildStrategicStructures(){
    const std::vector<MapPt>* lanes[3]={&laneTop(),&laneMid(),&laneBot()};
    for(auto* l:lanes){
        placeTowerOnLane(g_blueTowerMesh,*l,.20f,true);placeTowerOnLane(g_blueTowerMesh,*l,.34f,true);placeTowerOnLane(g_blueTowerMesh,*l,.46f,true);
        placeTowerOnLane(g_redTowerMesh,*l,.54f,false);placeTowerOnLane(g_redTowerMesh,*l,.66f,false);placeTowerOnLane(g_redTowerMesh,*l,.80f,false);
    }
    if(g_blueCoreMesh)addSceneMeshNatural(g_blueCoreMesh,-101.f,-101.f,.78f,true,g_medievalSRV);
    if(g_redCoreMesh)addSceneMeshNatural(g_redCoreMesh,101.f,101.f,-2.35f,true,g_medievalSRV);
    static const MapPt blueSlots[4]={{-104,-84},{-86,-104},{-82,-88},{-98,-72}};
    static const MapPt redSlots[4]={{104,84},{86,104},{82,88},{98,72}};
    for(size_t i=0;i<g_blueBaseBuildings.size()&&i<4;++i)addSceneMeshNatural(g_blueBaseBuildings[i],blueSlots[i].x,blueSlots[i].z,.78f,true,g_medievalSRV);
    for(size_t i=0;i<g_redBaseBuildings.size()&&i<4;++i)addSceneMeshNatural(g_redBaseBuildings[i],redSlots[i].x,redSlots[i].z,-2.35f,true,g_medievalSRV);
}
'@
$code=[regex]::Replace($code,$stratPattern,$strat,1)

# Jungla: masas ordenadas a ambos lados de rutas, sin props sueltos ni objetos en carriles.
$jungPattern='(?s)static void buildJungleWalls\(\)\{.*?\n\}(?=\s*static void buildBoundaryWalls)'
if(-not [regex]::IsMatch($code,$jungPattern)){throw 'No encontre buildJungleWalls.'}
$jung=@'
static void buildJungleWalls(){
    if(g_forestCurated.empty())return;
    const std::vector<MapPt>* paths[4]={&jungleBlueTop(),&jungleBlueBot(),&jungleRedTop(),&jungleRedBot()};
    size_t n=0;
    for(auto* path:paths){
        for(int i=0;i<9;++i){
            float yaw=0;MapPt p=samplePolyline(*path,(i+1.f)/10.f,yaw);
            for(int sideSign=-1;sideSign<=1;sideSign+=2){
                MapPt q=offsetPerp(p,yaw,sideSign*8.5f);
                if(inLane(q.x,q.z,2.2f)||inRiver(q.x,q.z,1.8f)||fabsf(q.x)>111.f||fabsf(q.z)>111.f)continue;
                MeshGPU* m=g_forestCurated[n++%g_forestCurated.size()];
                addSceneMeshNatural(m,q.x,q.z,yaw+(sideSign<0?0.f:XM_PI),true,g_forestSRV);
            }
        }
    }
}
'@
$code=[regex]::Replace($code,$jungPattern,$jung,1)

# Perimetro simple; mantener escala nativa y reducir draw calls.
$wallPattern='(?s)static void buildBoundaryWalls\(\)\{.*?\n\}(?=\s*static const std::vector<MapPt>& mobLane)'
if(-not [regex]::IsMatch($code,$wallPattern)){throw 'No encontre buildBoundaryWalls.'}
$wall=@'
static void buildBoundaryWalls(){
    if(!g_boundaryWallMesh)return;const float B=117.f;const int N=28;
    for(int i=0;i<=N;++i){float t=-B+(2.f*B*i/N);addSceneMeshNatural(g_boundaryWallMesh,t,-B,0,true,g_dungeonFullSRV);addSceneMeshNatural(g_boundaryWallMesh,t,B,XM_PI,true,g_dungeonFullSRV);addSceneMeshNatural(g_boundaryWallMesh,-B,t,XM_PIDIV2,true,g_dungeonFullSRV);addSceneMeshNatural(g_boundaryWallMesh,B,t,-XM_PIDIV2,true,g_dungeonFullSRV);}
}
'@
$code=[regex]::Replace($code,$wallPattern,$wall,1)

# Borrar toda colocacion heredada de v1-v18 antes de construir el escenario final.
$initOld='if(!buildHexFloor())return false;buildBridges();buildStrategicStructures();buildJungleWalls();buildBoundaryWalls();resetMobWave();'
$initNew='g_scene.clear();g_sceneRuntimeCount=(size_t)-1;for(auto& c:g_sceneGrid)c.clear();if(!buildHexFloor())return false;buildBridges();buildStrategicStructures();buildJungleWalls();buildBoundaryWalls();resetMobWave();'
if(-not $code.Contains($initOld)){throw 'No encontre secuencia final de construccion del mapa.'}
$code=$code.Replace($initOld,$initNew)

# Las torres tambien conservan la escala propia del pack.
$towerPattern='static void placeTowerOnLane\(MeshGPU\* m,const std::vector<MapPt>& lane,float u,bool blue\)\{.*?\}'
if(-not [regex]::IsMatch($code,$towerPattern)){throw 'No encontre placeTowerOnLane.'}
$tower='static void placeTowerOnLane(MeshGPU* m,const std::vector<MapPt>& lane,float u,bool blue){if(!m)return;float yaw=0;MapPt p=samplePolyline(lane,u,yaw);p=offsetPerp(p,yaw,blue?-6.4f:6.4f);addSceneMeshNatural(m,p.x,p.z,yaw,true,g_medievalSRV);}'
$code=[regex]::Replace($code,$towerPattern,$tower,1)

if($code -eq $original){throw 'El parche v20 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v20 aplicado: escena heredada descartada; edificios completos, bases, torres y jungla reconstruidos con colocacion fija y escala nativa.'
