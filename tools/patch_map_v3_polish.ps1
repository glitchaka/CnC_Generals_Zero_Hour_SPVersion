param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

RR 'const int N=181; const float SIZE=220.f' 'const int N=221; const float SIZE=340.f' 'terreno ampliado para el mapa completo'
RR 'bool rock=p.find(L"rock")!=std::wstring::npos||p.find(L"stone")!=std::wstring::npos;' 'bool rock=p.find(L"rock")!=std::wstring::npos||p.find(L"stone")!=std::wstring::npos; bool baseRelevant=p.find(L"castle")!=std::wstring::npos||p.find(L"townhall")!=std::wstring::npos||p.find(L"tower")!=std::wstring::npos||p.find(L"watchtower")!=std::wstring::npos||p.find(L"barracks")!=std::wstring::npos||p.find(L"blacksmith")!=std::wstring::npos||p.find(L"market")!=std::wstring::npos||p.find(L"shrine")!=std::wstring::npos||p.find(L"stables")!=std::wstring::npos||p.find(L"tent")!=std::wstring::npos;' 'seleccion de edificios de base'
RR 'if(building&&(blue||red)){' 'if(building&&(blue||red)&&baseRelevant){' 'densidad controlada de bases'
RR 'target=structural?2.5f:1.0f;coll=structural;' 'bool campDecor=p.find(L"barrel")!=std::wstring::npos||p.find(L"crate")!=std::wstring::npos||p.find(L"chest")!=std::wstring::npos||p.find(L"torch")!=std::wstring::npos||p.find(L"rocks")!=std::wstring::npos||p.find(L"barrier")!=std::wstring::npos||p.find(L"column")!=std::wstring::npos||p.find(L"scaffold")!=std::wstring::npos||p.find(L"cart")!=std::wstring::npos||p.find(L"weapon")!=std::wstring::npos;target=structural?2.5f:1.0f;coll=structural;' 'decoracion exterior de campamentos'
RR 'if(chooseCampSlot(h,target*.30f,x,z)){addSceneMesh(m,x,z,yaw,target,false,tex);return;}' 'if(campDecor&&chooseCampSlot(h,target*.30f,x,z)){addSceneMesh(m,x,z,yaw,target,false,tex);return;}' 'evitar muebles de dungeon amontonados en jungla'
RR 'float ang=((h/7)%6283)*.001f,rad=10.5f+(h%350)/100.f;x=cx+cosf(ang)*rad;z=cz+sinf(ang)*rad;target=1.0f;' 'float ang=((h/7)%6283)*.001f,rad=13.0f+(h%500)/100.f;x=cx+cosf(ang)*rad;z=cz+sinf(ang)*rad;target=.72f;' 'armerias despejadas'

$code=$code.Replace('g_player.pos={-68.f,groundH(-68.f,-68.f),-68.f};','g_player.pos={-92.f,groundH(-92.f,-92.f),-92.f};')
$code=$code.Replace('g_npc.pos={68.f,groundH(68.f,68.f),68.f};','g_npc.pos={92.f,groundH(92.f,92.f),92.f};')
$code=$code.Replace('g_player.home=g_player.pos;g_camTarget={g_player.pos.x,g_player.pos.y+1.f,g_player.pos.z};','g_player.home=g_player.pos;g_camTarget={g_player.pos.x,g_player.pos.y+1.f,g_player.pos.z};')

if($code -eq $original){throw 'El pulido del mapa no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v3: terreno ampliado, bases despejadas, campamentos legibles y spawns en base.'
