param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

# No reducir ninguna malla por debajo de su escala authored. El parametro target solo puede ampliar,
# nunca encoger un modelo que ya tiene su tamano natural. v7 ya cambia la formula para edificios,
# por eso este parche modifica solo el retorno y no depende de una firma vieja exacta.
if($code.Contains('return (std::max)(1.0f,target/e);')){
    # Ya preserva escala nativa.
} elseif($code.Contains('return target/e;}')){
    $code=$code.Replace('return target/e;}','return (std::max)(1.0f,target/e);}')
} else {
    throw 'No encontre el retorno de meshScale para preservar escala nativa.'
}

# Nunca mandar assets sobrantes fuera del mapa. Buscar una posicion interior libre y reutilizar la malla completa.
$oldScenic='static void scenicBelt(MeshGPU* m,uint32_t h,float target,ID3D11ShaderResourceView* tex){float a=(h%6283)*.001f;float band=(float)((h/41)%4);float rad=121.5f+band*7.5f+(float)((h/17)%500)/100.f;float x=cosf(a)*rad,z=sinf(a)*rad;addSceneMesh(m,x,z,-a,target,false,tex,{.92f,.94f,.98f,1});}'
$newScenic=@'
static void scenicBelt(MeshGPU* m,uint32_t h,float target,ID3D11ShaderResourceView* tex){
    if(!m)return;
    for(int a=0;a<80;++a){
        uint32_t k=h+0x9e3779b9u*(a+1);
        float x=((int)(k%20400)-10200)/100.f;
        float z=((int)((k/173u)%20400)-10200)/100.f;
        if(fabsf(x)>102.f||fabsf(z)>102.f)continue;
        if(blocksGameplay(x,z,target*.20f)||tooCloseToCollider(x,z,target*.28f))continue;
        addSceneMesh(m,x,z,(k%6283)*.001f,target,false,tex,{.92f,.94f,.98f,1});
        return;
    }
}
'@
if($code.Contains($oldScenic)){$code=$code.Replace($oldScenic,$newScenic)} else {
  $pattern='(?s)static void scenicBelt\(MeshGPU\* m,uint32_t h,float target,ID3D11ShaderResourceView\* tex\)\{.*?\}(?=\s*static )'
  if(-not [regex]::IsMatch($code,$pattern)){throw 'No encontre scenicBelt.'}
  $code=[regex]::Replace($code,$pattern,$newScenic,1)
}

# Los edificios medievales completos deben vivir dentro de las bases y zona jugable; no en el anillo exterior.
$oldBase='if(building&&(blue||red)&&baseRelevant){bool isTower=p.find(L"tower")!=std::wstring::npos;bool isCore=p.find(L"castle")!=std::wstring::npos||p.find(L"townhall")!=std::wstring::npos;float minH=isCore?18.0f:(isTower?12.0f:9.0f);float sy=(std::max)(1.0f,minH/(std::max)(.01f,m->bmax.y-m->bmin.y));float naturalR=(std::max)(1.0f,(std::max)(m->bmax.x-m->bmin.x,m->bmax.z-m->bmin.z)*sy*.38f);if(chooseBasePerimeter(h,blue,naturalR,x,z)){addSceneMeshArchitecture(m,x,z,yaw,minH,true,tex);return;}}'
$newBase='if(building&&(blue||red)){bool isTower=p.find(L"tower")!=std::wstring::npos;bool isCore=p.find(L"castle")!=std::wstring::npos||p.find(L"townhall")!=std::wstring::npos;float minH=isCore?18.0f:(isTower?12.0f:9.0f);float sy=(std::max)(1.0f,minH/(std::max)(.01f,m->bmax.y-m->bmin.y));float naturalR=(std::max)(1.0f,(std::max)(m->bmax.x-m->bmin.x,m->bmax.z-m->bmin.z)*sy*.38f);if(chooseBasePerimeter(h,blue,naturalR,x,z)){addSceneMeshArchitecture(m,x,z,yaw,minH,true,tex);return;}float bx=blue?-82.f:82.f,bz=blue?-82.f:82.f;float aa=((h/13u)%6283u)*.001f,rr=24.f+(float)(h%900u)/100.f;x=bx+cosf(aa)*rr;z=bz+sinf(aa)*rr;if(fabsf(x)<108.f&&fabsf(z)<108.f&&!inLane(x,z,naturalR*.3f)){addSceneMeshArchitecture(m,x,z,yaw,minH,true,tex);return;}}'
RR $oldBase $newBase 'edificios completos dentro del mapa'

# Nada del pack medieval que no sea camino/rio/edificio debe formar un basurero exterior.
RR 'scenicBelt(m,h,building?4.1f:1.8f,tex);return;' 'if(!building)scenicBelt(m,h,1.8f,tex);return;' 'descartar edificios sobrantes fuera de bases'

# Props de bosque/dungeon solo se colocan donde realmente construyen la jungla/objetivos.
# No se encogen: si entran, conservan su escala nativa o mayor.
$oldForest='if(kind==0){target=cliff?5.5f:(tree?4.1f:(plant?1.1f:(rock?2.0f:1.5f)));coll=tree||cliff||rock;float r=coll?target*.42f:.15f;bool active=(h%5u)==0u;if(active&&chooseJungleSlot(h,r,x,z)){addSceneMesh(m,x,z,yaw,target,coll,tex);return;}scenicBelt(m,h,target*.72f,tex);return;}'
$newForest='if(kind==0){target=cliff?5.5f:(tree?4.1f:(plant?1.1f:(rock?2.0f:1.5f)));coll=tree||cliff||rock;float r=coll?target*.42f:.15f;bool active=(h%9u)==0u;if(active&&chooseJungleSlot(h,r,x,z)){addSceneMesh(m,x,z,yaw,target,coll,tex);return;}return;}'
if($code.Contains($oldForest)){$code=$code.Replace($oldForest,$newForest)}

# Dungeon solo en objetivos/campamentos; nada de cinturones de basura.
$code=$code.Replace('scenicBelt(m,h,target,tex);return;}','return;}')

# Adventurer OBJ decorativos no se dispersan como basura por las bases: el personaje jugable ya usa sus GLB.
$code=$code.Replace('bool team=(h&1u)==0;float cx=team?-92.f:92.f,cz=team?-92.f:92.f;float ang=((h/7)%6283)*.001f,rad=10.5f+(h%350)/100.f;x=cx+cosf(ang)*rad;z=cz+sinf(ang)*rad;target=1.0f;addSceneMesh(m,x,z,yaw,target,false,tex);','return;')

if($code -eq $original){throw 'El parche v15 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v15 aplicado: escala nativa preservada; sin basurero exterior; edificios completos y decoracion dentro del area jugable.'
