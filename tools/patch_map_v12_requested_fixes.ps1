param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

# El piso debe seguir leyendo como las piezas hex_grass/rombos del pack, no como una lamina lisa.
# Se conserva la teselacion exacta sin huecos, pero se levantan suavemente los bordes para recuperar
# la silueta de cada pieza. Los hexes transitables usan exactamente la misma geometria, sin textura.
RR 'Vertex c{};c.p={cx,groundH(cx,cz)+.090f,cz};c.n={0,1,0};c.uv={cx*.045f,cz*.045f};dstV.push_back(c);' 'Vertex c{};c.p={cx,groundH(cx,cz)+.090f,cz};c.n={0,1,0};c.uv={cx*.045f,cz*.045f};dstV.push_back(c);' 'centro de celda'
RR 'for(int i=0;i<6;++i){float x=cx+hx[i]*S,z=cz+hz[i]*S;Vertex v{};v.p={x,groundH(x,z)+.090f,z};v.n={0,1,0};v.uv={x*.045f,z*.045f};dstV.push_back(v);}' 'for(int i=0;i<6;++i){float x=cx+hx[i]*S,z=cz+hz[i]*S;Vertex v{};v.p={x,groundH(x,z)+.105f,z};v.n={0,1,0};v.uv={x*.045f,z*.045f};dstV.push_back(v);}' 'relieve visible de las piezas de piso'

# Escala de arquitectura: edificios y torres se calculan por altura, nunca por la enorme huella X/Z.
# Los objetivos MOBA deben dominar claramente a un personaje humano.
RR 'float raw=target>=8.f?ey:(std::max)(ex,(std::max)(ey,ez));' 'float raw=target>=6.f?ey:(std::max)(ex,(std::max)(ey,ez));' 'escala arquitectonica por altura'
RR 'target=p.find(L"tower")!=std::wstring::npos?12.0f:16.0f;' 'target=p.find(L"tower")!=std::wstring::npos?18.0f:24.0f;' 'torres y edificios de base a escala normal'
RR 'addSceneMesh(g_blueCoreMesh,-106.f,-106.f,.78f,20.0f,true,g_medievalSRV);' 'addSceneMesh(g_blueCoreMesh,-106.f,-106.f,.78f,28.0f,true,g_medievalSRV);' 'nucleo azul a escala de edificio'
RR 'addSceneMesh(g_redCoreMesh,106.f,106.f,-2.35f,20.0f,true,g_medievalSRV);' 'addSceneMesh(g_redCoreMesh,106.f,106.f,-2.35f,28.0f,true,g_medievalSRV);' 'nucleo rojo a escala de edificio'
RR 'addSceneMesh(m,p.x,p.z,yaw,12.0f,true,g_medievalSRV);' 'addSceneMesh(m,p.x,p.z,yaw,18.0f,true,g_medievalSRV);' 'torres de carril a escala de torre'

# Demo MOBA: al ganar no se ofrece ni se exige reiniciar. La victoria es un estado terminal de muestra.
# Escape sigue permitiendo cerrar la demo; R no reinicia una victoria.
$code=$code.Replace('if(g_gameState==GameState::Victory&&keyPressed(''R''))resetGame();','')
$code=$code.Replace('if(g_gameState==GameState::Victory && keyPressed(''R'')) resetGame();','')
$code=$code.Replace('if(g_gameState==GameState::Victory){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.02f,.06f,.03f,.40f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("VICTORIA",cx-108.f,g_h*.5f-48.f,4.f,{.25f,.92f,.42f,1});drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});}','if(g_gameState==GameState::Victory){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.02f,.06f,.03f,.40f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("VICTORIA",cx-108.f,g_h*.5f-26.f,4.f,{.25f,.92f,.42f,1});}')

if($code -eq $original){throw 'El parche v12 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v12 aplicado: piezas de piso visibles y sin huecos, arquitectura a escala y victoria terminal sin reinicio.'
