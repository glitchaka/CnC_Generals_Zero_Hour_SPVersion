param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

# Arquitectura del mapa: nunca reducir edificios por debajo de una altura fisica creible.
$natural='static void addSceneMeshNatural(MeshGPU* m,float x,float z,float yaw,bool coll,ID3D11ShaderResourceView* tex,XMFLOAT4 tint={1,1,1,1},float yoff=0){if(!m)return;g_scene.push_back({m,x,z,yaw,1.0f,coll,tex,tint,yoff});}'
$arch=$natural+' static void addSceneMeshArchitecture(MeshGPU* m,float x,float z,float yaw,float minHeight,bool coll,ID3D11ShaderResourceView* tex,XMFLOAT4 tint={1,1,1,1},float yoff=0){if(!m)return;float h=(std::max)(.01f,m->bmax.y-m->bmin.y);float sc=(std::max)(1.0f,minHeight/h);g_scene.push_back({m,x,z,yaw,sc,coll,tex,tint,yoff});}'
RR $natural $arch 'helper de escala arquitectonica'

# Edificios de base: torres >=12u, castillos/ayuntamientos >=18u, resto >=9u.
$baseOld='if(building&&(blue||red)&&baseRelevant){float naturalR=(std::max)(1.0f,(std::max)(m->bmax.x-m->bmin.x,m->bmax.z-m->bmin.z)*.38f);if(chooseBasePerimeter(h,blue,naturalR,x,z)){addSceneMeshNatural(m,x,z,yaw,true,tex);return;}}'
$baseNew='if(building&&(blue||red)&&baseRelevant){bool isTower=p.find(L"tower")!=std::wstring::npos;bool isCore=p.find(L"castle")!=std::wstring::npos||p.find(L"townhall")!=std::wstring::npos;float minH=isCore?18.0f:(isTower?12.0f:9.0f);float sy=(std::max)(1.0f,minH/(std::max)(.01f,m->bmax.y-m->bmin.y));float naturalR=(std::max)(1.0f,(std::max)(m->bmax.x-m->bmin.x,m->bmax.z-m->bmin.z)*sy*.38f);if(chooseBasePerimeter(h,blue,naturalR,x,z)){addSceneMeshArchitecture(m,x,z,yaw,minH,true,tex);return;}}'
RR $baseOld $baseNew 'edificios de base con altura real'

$towerOld='static void placeTowerOnLane(MeshGPU* m,const std::vector<MapPt>& lane,float u,bool blue){if(!m)return;float yaw=0;MapPt p=samplePolyline(lane,u,yaw);p=offsetPerp(p,yaw,blue?-5.2f:5.2f);addSceneMeshNatural(m,p.x,p.z,yaw,true,g_medievalSRV);}'
$towerNew='static void placeTowerOnLane(MeshGPU* m,const std::vector<MapPt>& lane,float u,bool blue){if(!m)return;float yaw=0;MapPt p=samplePolyline(lane,u,yaw);p=offsetPerp(p,yaw,blue?-6.6f:6.6f);addSceneMeshArchitecture(m,p.x,p.z,yaw,12.0f,true,g_medievalSRV);}'
RR $towerOld $towerNew 'torres de carril con escala de edificio'

RR 'addSceneMeshNatural(g_blueCoreMesh,-106.f,-106.f,.78f,true,g_medievalSRV);' 'addSceneMeshArchitecture(g_blueCoreMesh,-106.f,-106.f,.78f,18.0f,true,g_medievalSRV);' 'nucleo azul grande'
RR 'addSceneMeshNatural(g_redCoreMesh,106.f,106.f,-2.35f,true,g_medievalSRV);' 'addSceneMeshArchitecture(g_redCoreMesh,106.f,106.f,-2.35f,18.0f,true,g_medievalSRV);' 'nucleo rojo grande'

# Jungla: bajar el ruido visual alrededor de carriles y mantener vegetacion/rocas fuera de las rutas.
$slotOld='if(kind==0){target=cliff?5.5f:(tree?4.1f:(plant?1.1f:(rock?2.0f:1.5f)));coll=tree||cliff||rock;float r=coll?target*.42f:.15f;bool active=(h%8u)==0u;if(active&&chooseJungleSlot(h,r,x,z)){addSceneMesh(m,x,z,yaw,target,coll,tex);return;}scenicBelt(m,h,target*.82f,tex);return;}'
$slotNew='if(kind==0){target=cliff?5.5f:(tree?4.1f:(plant?1.1f:(rock?2.0f:1.5f)));coll=tree||cliff||rock;float r=coll?target*.42f:.15f;bool active=(h%5u)==0u;if(active&&chooseJungleSlot(h,r,x,z)){addSceneMesh(m,x,z,yaw,target,coll,tex);return;}scenicBelt(m,h,target*.72f,tex);return;}'
if($code.Contains($slotOld)){$code=$code.Replace($slotOld,$slotNew)}

# Limite del mapa: el muro tambien debe tener altura arquitectonica, no parecer una baranda.
$wallOld='static void buildBoundaryWalls(){if(!g_boundaryWallMesh)return;const float B=118.f;const int N=52;for(int i=0;i<=N;++i){float t=-B+(2.f*B*i/N);addSceneMesh(g_boundaryWallMesh,t,-B,0,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,t,B,XM_PI,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,-B,t,XM_PIDIV2,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,B,t,-XM_PIDIV2,7.5f,true,g_medievalSRV);}}'
$wallNew='static void buildBoundaryWalls(){if(!g_boundaryWallMesh)return;const float B=118.f;const int N=52;for(int i=0;i<=N;++i){float t=-B+(2.f*B*i/N);addSceneMeshArchitecture(g_boundaryWallMesh,t,-B,0,6.5f,true,g_medievalSRV);addSceneMeshArchitecture(g_boundaryWallMesh,t,B,XM_PI,6.5f,true,g_medievalSRV);addSceneMeshArchitecture(g_boundaryWallMesh,-B,t,XM_PIDIV2,6.5f,true,g_medievalSRV);addSceneMeshArchitecture(g_boundaryWallMesh,B,t,-XM_PIDIV2,6.5f,true,g_medievalSRV);}}'
if($code.Contains($wallOld)){$code=$code.Replace($wallOld,$wallNew)}

# El mapa debe seguir siendo continuo y transitable: piso de hexes completo, rutas sin textura y sin lamina plana antigua.
if($code.Contains('drawMeshWorld(g_terrain,XMMatrixIdentity()')){throw 'El terreno plano antiguo sigue activo en el render.'}
if(-not $code.Contains('drawMeshWorld(g_hexFloorTextured')){throw 'Falta el piso hexagonal texturizado.'}
if(-not $code.Contains('drawMeshWorld(g_hexFloorTransit')){throw 'Falta el piso hexagonal transitable.'}

if($code -eq $original){throw 'El parche v13 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v13 aplicado: edificios, torres, nucleos y muro con escala arquitectonica; jungla legible; piso hexagonal continuo preservado.'
& (Join-Path $PSScriptRoot 'patch_map_v14_runtime_budget.ps1') -SourcePath $SourcePath
