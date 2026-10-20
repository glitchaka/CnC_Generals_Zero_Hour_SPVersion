param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

# El gameplay v7 llama resolveCharacter antes de su definicion final. Declararlo antes del uso.
if(-not $code.Contains('static void resolveCharacter(Character& c);')){
    RR 'static void resolvePlayerMobCollision(){' "static void resolveCharacter(Character& c);`r`nstatic void resolvePlayerMobCollision(){" 'declaracion adelantada de resolveCharacter'
}

# View/projection se calculaban otra vez por CADA malla dibujada. Se calculan una vez por frame.
RR 'static XMFLOAT3 g_camTarget{0,1,0},g_camEye{0,8,-10};' 'static XMFLOAT3 g_camTarget{0,1,0},g_camEye{0,8,-10}; static XMMATRIX g_frameView{},g_frameProj{};' 'matrices de camara por frame'
RR 'XMVECTOR eye=XMLoadFloat3(&g_camEye),at=XMLoadFloat3(&g_camTarget),up=XMVectorSet(0,1,0,0); XMMATRIX view=XMMatrixLookAtLH(eye,at,up),proj=XMMatrixPerspectiveFovLH(XMConvertToRadians(48.f),g_w/(float)(std::max)(1,g_h),.1f,400.f);' 'XMMATRIX view=g_frameView,proj=g_frameProj;' 'evitar recalculo de camara por draw call'
RR 'if(!g_rtv)return; float clear[]={.48f,.62f,.70f,1};' 'if(!g_rtv)return; XMVECTOR frameEye=XMLoadFloat3(&g_camEye),frameAt=XMLoadFloat3(&g_camTarget),frameUp=XMVectorSet(0,1,0,0);g_frameView=XMMatrixLookAtLH(frameEye,frameAt,frameUp);g_frameProj=XMMatrixPerspectiveFovLH(XMConvertToRadians(48.f),g_w/(float)(std::max)(1,g_h),.1f,400.f); float clear[]={.48f,.62f,.70f,1};' 'calculo unico de view y projection'

# Occlusion de camara: cualquier modelo que intercepte realmente el segmento visual camara-jugador
# desaparece mientras tapa al personaje. Se comprueba huella y altura, no solo distancia 2D.
$anchor='static XMMATRIX mobWorld(const Mob& m){'
$helper=@'
static bool sceneObjectVisible(const SceneObj& o){
    if(!o.mesh)return false;
    float tx=o.x-g_camTarget.x,tz=o.z-g_camTarget.z;
    if(tx*tx+tz*tz>8100.f)return false;
    float fdx=g_camTarget.x-g_camEye.x,fdz=g_camTarget.z-g_camEye.z;
    float odx=o.x-g_camEye.x,odz=o.z-g_camEye.z;
    if(fdx*odx+fdz*odz<0.f)return false;

    const float px=g_player.pos.x,pz=g_player.pos.z,py=g_player.pos.y+.85f;
    float sx=g_camEye.x-px,sz=g_camEye.z-pz,seg2=sx*sx+sz*sz;
    if(seg2>1e-5f){
        float ox=o.x-px,oz=o.z-pz;
        float t=(ox*sx+oz*sz)/seg2;
        if(t>.025f&&t<.97f){
            float qx=px+sx*t,qz=pz+sz*t,dx=o.x-qx,dz=o.z-qz;
            float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.50f;
            float rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.50f;
            float r=clampf((std::max)(rx,rz),.45f,12.f);
            if(dx*dx+dz*dz<(r+.38f)*(r+.38f)){
                float baseY=groundH(o.x,o.z)+o.yOffset;
                float y0=baseY+o.mesh->bmin.y*o.scale;
                float y1=baseY+o.mesh->bmax.y*o.scale;
                if(y0>y1)std::swap(y0,y1);
                float rayY=py+(g_camEye.y-py)*t;
                if(rayY>=y0-.25f&&rayY<=y1+.35f)return false;
            }
        }
    }
    return true;
}

static XMMATRIX mobWorld(const Mob& m){
'@
RR $anchor $helper 'occlusion entre camara y jugador'

# Sustituir el culling circular bruto por distancia + descarte tras camara + occlusion del jugador.
$oldRender='for(const auto& o:g_scene){float dx=o.x-g_camTarget.x,dz=o.z-g_camTarget.z;if(dx*dx+dz*dz<9000.f)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);}'
if(-not $code.Contains($oldRender)){
    $oldRender='for(const auto& o:g_scene){float dx=o.x-g_camTarget.x,dz=o.z-g_camTarget.z;if(dx*dx+dz*dz<15000.f)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);}'
}
RR $oldRender 'for(const auto& o:g_scene)if(sceneObjectVisible(o))drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);' 'render con occlusion y culling'

# Broad phase barata para colisiones de escenario: evitar calculos de bounds para objetos lejanos.
$collisionOld='const float cr=.42f; for(const auto& o:g_scene){ if(!o.collider||!o.mesh)continue; float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.42f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.42f,r=(std::max)(.55f,(std::max)(rx,rz)); float dx=c.pos.x-o.x,dz=c.pos.z-o.z,ds=dx*dx+dz*dz,rr=(r+cr)*(r+cr);'
$collisionNew='const float cr=.42f; for(const auto& o:g_scene){ if(!o.collider||!o.mesh)continue; float dx=c.pos.x-o.x,dz=c.pos.z-o.z;if(fabsf(dx)>28.f||fabsf(dz)>28.f)continue;float rx=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.42f,rz=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.42f,r=(std::max)(.55f,(std::max)(rx,rz)); float ds=dx*dx+dz*dz,rr=(r+cr)*(r+cr);'
RR $collisionOld $collisionNew 'broad phase de colisiones'

# Present(1) ya sincroniza con refresco; Sleep(1) solo agrega latencia/jitter.
$code=$code.Replace('render();Sleep(1);','render();')

if($code -eq $original){throw 'El parche v16 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v16 aplicado: modelos que tapan al jugador desaparecen; culling mas estricto; matrices de camara por frame; colisiones y loop optimizados.'
