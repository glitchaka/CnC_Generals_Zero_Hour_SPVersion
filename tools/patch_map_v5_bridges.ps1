param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

RR 'static std::vector<MeshGPU*> g_laneTiles,g_riverTiles;' 'static std::vector<MeshGPU*> g_laneTiles,g_riverTiles,g_bridgeTiles;' 'vector de puentes'
RR 'bool road=p.find(L"\\tiles\\roads\\")!=std::wstring::npos||p.find(L"hex_road_")!=std::wstring::npos||p.find(L"building_bridge_")!=std::wstring::npos;' 'bool bridge=p.find(L"building_bridge_")!=std::wstring::npos; bool road=p.find(L"\\tiles\\roads\\")!=std::wstring::npos||p.find(L"hex_road_")!=std::wstring::npos;' 'separar puentes de caminos'
RR 'if(kind==1){rememberStrategicMesh(p,m);if(road){g_laneTiles.push_back(m);scenicBelt(m,h,2.2f,tex);return;}' 'if(kind==1){rememberStrategicMesh(p,m);if(bridge){g_bridgeTiles.push_back(m);scenicBelt(m,h,2.8f,tex);return;}if(road){g_laneTiles.push_back(m);scenicBelt(m,h,2.2f,tex);return;}' 'captura de puentes'
RR 'g_worldOwned.clear();g_laneTiles.clear();g_riverTiles.clear();g_blueTowerMesh=g_redTowerMesh=g_blueCoreMesh=g_redCoreMesh=nullptr;' 'g_worldOwned.clear();g_laneTiles.clear();g_riverTiles.clear();g_bridgeTiles.clear();g_blueTowerMesh=g_redTowerMesh=g_blueCoreMesh=g_redCoreMesh=nullptr;' 'limpieza de puentes'

$anchor='static void buildRiverRibbon(){if(g_riverTiles.empty())return;const int samples=34;for(int i=0;i<samples;++i){float yaw=0;MapPt p=samplePolyline(riverLine(),(i+.5f)/samples,yaw);MeshGPU* m=g_riverTiles[i%g_riverTiles.size()];addSceneMesh(m,p.x,p.z,yaw,5.4f,false,g_medievalSRV,{.68f,.82f,.96f,1},-.035f);}}'
$with=@'
static void buildRiverRibbon(){if(g_riverTiles.empty())return;const int samples=34;for(int i=0;i<samples;++i){float yaw=0;MapPt p=samplePolyline(riverLine(),(i+.5f)/samples,yaw);MeshGPU* m=g_riverTiles[i%g_riverTiles.size()];addSceneMesh(m,p.x,p.z,yaw,5.4f,false,g_medievalSRV,{.68f,.82f,.96f,1},-.035f);}}
static void buildBridges(){if(g_bridgeTiles.empty())return;MapPt p[3]={{-99.f,50.f},{0.f,0.f},{50.f,-99.f}};float y[3]={0.f,XM_PIDIV4,XM_PIDIV2};for(int i=0;i<3;++i)addSceneMesh(g_bridgeTiles[i%g_bridgeTiles.size()],p[i].x,p[i].z,y[i],7.0f,false,g_medievalSRV,{1,1,1,1},.08f);}
'@
RR $anchor $with 'construccion de cruces del rio'
RR 'buildLaneRibbon(laneTop(),0);buildLaneRibbon(laneMid(),1);buildLaneRibbon(laneBot(),2);buildRiverRibbon();buildStrategicStructures();buildJungleWalls();' 'buildLaneRibbon(laneTop(),0);buildLaneRibbon(laneMid(),1);buildLaneRibbon(laneBot(),2);buildRiverRibbon();buildBridges();buildStrategicStructures();buildJungleWalls();' 'activar puentes solo en cruces'

if($code -eq $original){throw 'El parche de puentes no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Puentes separados del pavimento: solo aparecen en cruces de rio.'
