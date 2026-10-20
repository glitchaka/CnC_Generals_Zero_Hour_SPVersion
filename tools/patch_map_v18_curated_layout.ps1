param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

# Eliminar definitivamente la distribucion automatica de basura: cada pack tiene un papel concreto.
$placePattern='(?s)static void placeOriginalAsset\(MeshGPU\* m,const std::wstring& p,uint32_t h,int kind,ID3D11ShaderResourceView\* tex\)\{.*?\n\}(?=\s*static bool runtimeAssetWanted)'
if(-not [regex]::IsMatch($code,$placePattern)){throw 'No encontre placeOriginalAsset para reconstruir el layout.'}
$place=@'
static void placeOriginalAsset(MeshGPU* m,const std::wstring& p,uint32_t h,int kind,ID3D11ShaderResourceView* tex){
    if(!m)return;
    if(kind==2&&!g_boundaryWallMesh&&(p.find(L"\\wall.obj")!=std::wstring::npos||p.find(L"wall_arched.obj")!=std::wstring::npos||p.find(L"wall_half.obj")!=std::wstring::npos))g_boundaryWallMesh=m;
    bool bridge=p.find(L"building_bridge_")!=std::wstring::npos;
    bool road=p.find(L"\\tiles\\roads\\")!=std::wstring::npos||p.find(L"hex_road_")!=std::wstring::npos;
    bool water=p.find(L"\\tiles\\rivers\\")!=std::wstring::npos||p.find(L"hex_water")!=std::wstring::npos;
    bool building=p.find(L"building_")!=std::wstring::npos||p.find(L"house")!=std::wstring::npos||p.find(L"tower")!=std::wstring::npos||p.find(L"castle")!=std::wstring::npos||p.find(L"townhall")!=std::wstring::npos||p.find(L"gate")!=std::wstring::npos;
    bool blue=p.find(L"blue")!=std::wstring::npos,red=p.find(L"red")!=std::wstring::npos;
    bool tower=p.find(L"tower")!=std::wstring::npos;
    bool core=p.find(L"castle")!=std::wstring::npos||p.find(L"townhall")!=std::wstring::npos;
    bool tree=p.find(L"tree")!=std::wstring::npos;
    bool rock=p.find(L"rock")!=std::wstring::npos||p.find(L"stone")!=std::wstring::npos||p.find(L"cliff")!=std::wstring::npos;
    float x=0,z=0,yaw=(h%6283u)*.001f;

    if(kind==1){
        rememberStrategicMesh(p,m);
        if(bridge){g_bridgeTiles.push_back(m);return;}
        if(road){g_laneTiles.push_back(m);return;}
        if(water){g_riverTiles.push_back(m);return;}
        if(!building)return;
        if(tower||core)return; // torres y nucleos se componen una sola vez en buildStrategicStructures.
        if(!(blue||red))return;
        if((h%3u)!=0u)return; // solo edificios de apoyo, no todo el pack.
        bool teamBlue=blue&&!red;float bx=teamBlue?-92.f:92.f,bz=teamBlue?-92.f:92.f;
        static const MapPt ring[8]={{-18,-5},{-15,10},{-6,18},{9,16},{18,5},{15,-10},{6,-18},{-9,-16}};
        uint32_t slot=(h/17u)%8u;float ox=ring[slot].x,oz=ring[slot].z;if(!teamBlue){ox=-ox;oz=-oz;}
        x=bx+ox;z=bz+oz;if(fabsf(x)>113.f||fabsf(z)>113.f||inLane(x,z,2.f)||tooCloseToCollider(x,z,2.5f))return;
        addSceneMeshArchitecture(m,x,z,teamBlue?.78f:-2.35f,9.0f,true,tex);return;
    }

    if(kind==0){
        if(!(tree||rock))return;
        if((h%4u)!=0u)return; // modelos semilla; buildJungleWalls compone las masas vegetales.
        const std::vector<MapPt>* paths[4]={&jungleBlueTop(),&jungleBlueBot(),&jungleRedTop(),&jungleRedBot()};
        const auto& path=*paths[(h/11u)%4u];float pyaw=0;MapPt q=samplePolyline(path,.10f+((h/37u)%800u)/1000.f,pyaw);float side=((h>>5)&1u)?9.5f:-9.5f;q=offsetPerp(q,pyaw,side);
        if(inLane(q.x,q.z,1.5f)||inRiver(q.x,q.z,1.f)||tooCloseToCollider(q.x,q.z,1.6f))return;
        if(tree)addSceneMeshNatural(m,q.x,q.z,pyaw,true,tex);else addSceneMesh(m,q.x,q.z,pyaw,2.2f,true,tex);return;
    }

    if(kind==2){
        bool wall=p.find(L"wall")!=std::wstring::npos||p.find(L"barrier")!=std::wstring::npos||p.find(L"column")!=std::wstring::npos;
        bool camp=p.find(L"crate")!=std::wstring::npos||p.find(L"barrel")!=std::wstring::npos||p.find(L"chest")!=std::wstring::npos||p.find(L"torch")!=std::wstring::npos||p.find(L"rocks")!=std::wstring::npos;
        if(wall){if((h%3u)!=0u)return;if(chooseObjectivePerimeter(h,(h&1u)==0,1.2f,x,z))addSceneMeshNatural(m,x,z,yaw,true,tex);return;}
        if(camp){if((h%2u)!=0u)return;if(chooseCampSlot(h,.5f,x,z))addSceneMeshNatural(m,x,z,yaw,false,tex);return;}
        return;
    }

    // El pack Adventurers se usa para personajes/equipo; no se dispersan armas como decoracion.
    return;
}
'@
$code=[regex]::Replace($code,$placePattern,$place,1)

# Cargar solo las mallas que realmente aparecen. Menos parseo OBJ, RAM y VRAM durante el arranque.
$wantedPattern='(?s)static bool runtimeAssetWanted\(const std::wstring& p,int kind,size_t kept\)\{.*?\n\}(?=\s*static void placeFullPack)'
if(-not [regex]::IsMatch($code,$wantedPattern)){throw 'No encontre runtimeAssetWanted.'}
$wanted=@'
static bool runtimeAssetWanted(const std::wstring& p,int kind,size_t kept){
    if(kind==1){
        bool strategic=p.find(L"tower")!=std::wstring::npos||p.find(L"castle")!=std::wstring::npos||p.find(L"townhall")!=std::wstring::npos;
        bool base=p.find(L"building_")!=std::wstring::npos&&(p.find(L"blue")!=std::wstring::npos||p.find(L"red")!=std::wstring::npos);
        bool lane=p.find(L"\\tiles\\roads\\")!=std::wstring::npos||p.find(L"hex_road_")!=std::wstring::npos||p.find(L"bridge")!=std::wstring::npos||p.find(L"\\tiles\\rivers\\")!=std::wstring::npos||p.find(L"hex_water")!=std::wstring::npos;
        return strategic||lane||(base&&kept<72);
    }
    if(kind==0){bool useful=p.find(L"tree")!=std::wstring::npos||p.find(L"rock")!=std::wstring::npos||p.find(L"stone")!=std::wstring::npos||p.find(L"cliff")!=std::wstring::npos;return useful&&kept<28;}
    if(kind==2){bool useful=p.find(L"wall")!=std::wstring::npos||p.find(L"column")!=std::wstring::npos||p.find(L"barrier")!=std::wstring::npos||p.find(L"rock")!=std::wstring::npos||p.find(L"torch")!=std::wstring::npos||p.find(L"chest")!=std::wstring::npos||p.find(L"crate")!=std::wstring::npos||p.find(L"barrel")!=std::wstring::npos;return useful&&kept<24;}
    return false;
}
'@
$code=[regex]::Replace($code,$wantedPattern,$wanted,1)

# El muro anterior hacia 324 instancias. Mantener un perimetro visible con muchas menos draw calls.
$wallPattern='(?s)static void buildBoundaryWalls\(\)\{.*?\}(?=\s*static const std::vector<MapPt>& mobLane)'
if(-not [regex]::IsMatch($code,$wallPattern)){throw 'No encontre buildBoundaryWalls.'}
$wall=@'
static void buildBoundaryWalls(){
    if(!g_boundaryWallMesh)return;const float B=118.f;const int N=34;
    for(int i=0;i<=N;++i){float t=-B+(2.f*B*i/N);addSceneMeshArchitecture(g_boundaryWallMesh,t,-B,0,6.5f,true,g_dungeonFullSRV);addSceneMeshArchitecture(g_boundaryWallMesh,t,B,XM_PI,6.5f,true,g_dungeonFullSRV);addSceneMeshArchitecture(g_boundaryWallMesh,-B,t,XM_PIDIV2,6.5f,true,g_dungeonFullSRV);addSceneMeshArchitecture(g_boundaryWallMesh,B,t,-XM_PIDIV2,6.5f,true,g_dungeonFullSRV);}
}
'@
$code=[regex]::Replace($code,$wallPattern,$wall,1)

# Menos repeticion de jungla: masas claras alrededor de caminos, no una alfombra de props.
$code=$code.Replace('for(auto* path:paths){for(int i=0;i<18;++i){','for(auto* path:paths){for(int i=0;i<12;++i){')
$code=$code.Replace('(i+.5f)/18.f','(i+.5f)/12.f')

# Nuevo presupuesto de carga coherente con el layout curado.
$code=$code.Replace('return g_worldOwned.size()>40&&g_laneTiles.size()>0&&g_blueTowerMesh&&g_redTowerMesh&&g_blueCoreMesh&&g_redCoreMesh;','return g_worldOwned.size()>20&&g_laneTiles.size()>0&&g_blueTowerMesh&&g_redTowerMesh&&g_blueCoreMesh&&g_redCoreMesh;')

if($code -eq $original){throw 'El parche v18 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v18 aplicado: layout curado, sin dispersion automatica; bases compuestas, jungla legible, objetivos claros y menor presupuesto de draw calls.'
