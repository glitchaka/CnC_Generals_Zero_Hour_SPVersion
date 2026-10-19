param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

$old='static void buildBoundaryWalls(){
    if(!g_boundaryWallMesh)return;const float B=118.f;const int N=52;for(int i=0;i<=N;++i){float t=-B+(2.f*B*i/N);addSceneMesh(g_boundaryWallMesh,t,-B,0,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,t,B,XM_PI,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,-B,t,XM_PIDIV2,7.5f,true,g_medievalSRV);addSceneMesh(g_boundaryWallMesh,B,t,-XM_PIDIV2,7.5f,true,g_medievalSRV);}}'
$new='static void buildBoundaryWalls(){
    if(!g_boundaryWallMesh)return;const float B=118.f;const int N=80;for(int i=0;i<=N;++i){float t=-B+(2.f*B*i/N);addSceneMesh(g_boundaryWallMesh,t,-B,0,8.5f,true,g_dungeonFullSRV);addSceneMesh(g_boundaryWallMesh,t,B,XM_PI,8.5f,true,g_dungeonFullSRV);addSceneMesh(g_boundaryWallMesh,-B,t,XM_PIDIV2,8.5f,true,g_dungeonFullSRV);addSceneMesh(g_boundaryWallMesh,B,t,-XM_PIDIV2,8.5f,true,g_dungeonFullSRV);}}'
if(-not $code.Contains($old)){throw 'No encontre buildBoundaryWalls actual.'}
$code=$code.Replace($old,$new)

# Mantener jugador y NPCs siempre dentro del anillo visual. El clamp queda un poco por dentro
# del muro para impedir que la camara atraviese la geometria exterior durante colisiones.
$code=$code.Replace('clampf(c.pos.x,-116.f,116.f)','clampf(c.pos.x,-113.5f,113.5f)')
$code=$code.Replace('clampf(c.pos.z,-116.f,116.f)','clampf(c.pos.z,-113.5f,113.5f)')

if($code -eq $original){throw 'El parche v11 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v11 aplicado: muro Dungeon continuo, texturizado y limite fisico dentro del perimetro.'
