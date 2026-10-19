param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

$oldLane='static void buildLaneRibbon(const std::vector<MapPt>& lane,int laneIndex){if(g_laneTiles.empty())return;const int samples=46;for(int i=0;i<samples;++i){float yaw=0;MapPt p=samplePolyline(lane,(i+.5f)/samples,yaw);MeshGPU* m=g_laneTiles[(i+laneIndex*7)%g_laneTiles.size()];addSceneMesh(m,p.x,p.z,yaw,5.6f,false,g_medievalSRV,{.92f,.90f,.82f,1},.035f);}}'
$oldRiver='static void buildRiverRibbon(){if(g_riverTiles.empty())return;const int samples=34;for(int i=0;i<samples;++i){float yaw=0;MapPt p=samplePolyline(riverLine(),(i+.5f)/samples,yaw);MeshGPU* m=g_riverTiles[i%g_riverTiles.size()];addSceneMesh(m,p.x,p.z,yaw,5.4f,false,g_medievalSRV,{.68f,.82f,.96f,1},-.035f);}}'
if(-not $code.Contains($oldLane)){throw 'No encontre buildLaneRibbon anterior.'}
if(-not $code.Contains($oldRiver)){throw 'No encontre buildRiverRibbon anterior.'}

$new=@'
static void buildGroundRibbon(const std::vector<MapPt>& line,float halfWidth,const XMFLOAT4& tint,float yBias){
    const int samples=112;std::vector<Vertex> v;std::vector<uint32_t> idx;v.reserve(samples*2);idx.reserve((samples-1)*6);
    for(int i=0;i<samples;++i){float u=i/(float)(samples-1),yaw=0;MapPt c=samplePolyline(line,u,yaw);MapPt l=offsetPerp(c,yaw,-halfWidth),r=offsetPerp(c,yaw,halfWidth);float vy=u*22.f;
        v.push_back({{l.x,groundH(l.x,l.z)+yBias,l.z},{0,1,0},{vy,0}});v.push_back({{r.x,groundH(r.x,r.z)+yBias,r.z},{0,1,0},{vy,1}});
        if(i>0){uint32_t a=(uint32_t)((i-1)*2),b=a+1,c0=(uint32_t)(i*2),d=c0+1;idx.insert(idx.end(),{a,c0,b,b,c0,d});}}
    MeshGPU* m=new MeshGPU;if(!makeMesh(v,idx,*m,false)){delete m;return;}g_worldOwned.push_back(m);g_scene.push_back({m,0,0,0,1.f,false,g_groundSRV,tint,-groundH(0,0)});
}
static void buildLaneRibbon(const std::vector<MapPt>& lane,int laneIndex){
    // Los carriles son superficies continuas de terreno, no una fila de piezas/rombos del atlas.
    XMFLOAT4 tint=laneIndex==1?XMFLOAT4{.74f,.70f,.61f,1}:XMFLOAT4{.70f,.67f,.58f,1};buildGroundRibbon(lane,7.0f,tint,.045f);
}
static void buildRiverRibbon(){
    // Cinta continua rebajada y azulada; evita repetir piezas sueltas que parecian marcadores amarillos.
    buildGroundRibbon(riverLine(),5.0f,{.42f,.58f,.66f,1},-.035f);
}
'@
$code=$code.Replace($oldLane,$new)
$code=$code.Replace($oldRiver,'')

# Los OBJ de camino/agua del pack siguen presentes para cumplir el uso completo del pack,
# pero permanecen como decoracion del perimetro y nunca vuelven a dibujarse como balizas del carril.
if($code -eq $original){throw 'El parche v8 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Carriles corregidos: superficies continuas, sin rombos amarillos ni piezas repetidas como marcadores.'
