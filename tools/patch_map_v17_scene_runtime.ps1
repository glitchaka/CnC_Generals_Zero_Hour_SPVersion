param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

# Runtime cache para escenario estatico: transforms, bounds y rejilla espacial se calculan una sola vez.
$camGlobals='static XMFLOAT3 g_camTarget{0,1,0},g_camEye{0,8,-10}; static XMMATRIX g_frameView{},g_frameProj{};'
$runtimeGlobals=@'
static XMFLOAT3 g_camTarget{0,1,0},g_camEye{0,8,-10}; static XMMATRIX g_frameView{},g_frameProj{};
struct SceneRuntime { XMMATRIX world{}; float radius=.5f,y0=0,y1=0; int cx=0,cz=0; };
static constexpr int SCENE_GRID_N=16;
static constexpr float SCENE_GRID_MIN=-128.f;
static constexpr float SCENE_GRID_SIZE=16.f;
static std::array<std::vector<uint32_t>,SCENE_GRID_N*SCENE_GRID_N> g_sceneGrid;
static std::vector<SceneRuntime> g_sceneRuntime;
static size_t g_sceneRuntimeCount=(size_t)-1;
static ID3D11Buffer* g_lastWorldVB=nullptr;
static ID3D11Buffer* g_lastWorldIB=nullptr;
static ID3D11ShaderResourceView* g_lastWorldSRV=nullptr;
'@
if(-not $code.Contains($camGlobals)){throw 'No encontre globals de camara de v16.'}
$code=$code.Replace($camGlobals,$runtimeGlobals)

# Cachear estados D3D repetidos entre instancias consecutivas de la misma malla/material.
$oldDraw='if(!m.vb||!m.ib||!m.count)return; UINT st=sizeof(Vertex),off=0; g_ctx->IASetVertexBuffers(0,1,&m.vb,&st,&off); g_ctx->IASetIndexBuffer(m.ib,DXGI_FORMAT_R32_UINT,0); g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);'
$newDraw='if(!m.vb||!m.ib||!m.count)return; UINT st=sizeof(Vertex),off=0; if(g_lastWorldVB!=m.vb){g_ctx->IASetVertexBuffers(0,1,&m.vb,&st,&off);g_lastWorldVB=m.vb;} if(g_lastWorldIB!=m.ib){g_ctx->IASetIndexBuffer(m.ib,DXGI_FORMAT_R32_UINT,0);g_lastWorldIB=m.ib;} g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);'
if(-not $code.Contains($oldDraw)){throw 'No encontre setup de drawMeshWorld.'}
$code=$code.Replace($oldDraw,$newDraw)
$oldSrv='g_ctx->UpdateSubresource(g_cb,0,nullptr,&cb,0,0);g_ctx->VSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetShaderResources(0,1,&srv);g_ctx->PSSetSamplers(0,1,&g_sampler);g_ctx->DrawIndexed(m.count,0,0);'
$newSrv='g_ctx->UpdateSubresource(g_cb,0,nullptr,&cb,0,0);g_ctx->VSSetConstantBuffers(0,1,&g_cb);g_ctx->PSSetConstantBuffers(0,1,&g_cb);if(g_lastWorldSRV!=srv){g_ctx->PSSetShaderResources(0,1,&srv);g_lastWorldSRV=srv;}g_ctx->PSSetSamplers(0,1,&g_sampler);g_ctx->DrawIndexed(m.count,0,0);'
if(-not $code.Contains($oldSrv)){throw 'No encontre binding de textura de drawMeshWorld.'}
$code=$code.Replace($oldSrv,$newSrv)

# Sustituir test de visibilidad por runtime precomputado y rejilla espacial.
$pattern='(?s)static bool sceneObjectVisible\(const SceneObj& o\)\{.*?\}(?=\s*static XMMATRIX mobWorld)'
if(-not [regex]::IsMatch($code,$pattern)){throw 'No encontre sceneObjectVisible de v16.'}
$runtime=@'
static int sceneCell(float v){return (std::max)(0,(std::min)(SCENE_GRID_N-1,(int)floorf((v-SCENE_GRID_MIN)/SCENE_GRID_SIZE)));}
static void rebuildSceneRuntime(){
    if(g_sceneRuntimeCount==g_scene.size())return;
    for(auto& c:g_sceneGrid)c.clear();
    g_sceneRuntime.clear();g_sceneRuntime.resize(g_scene.size());
    for(uint32_t i=0;i<(uint32_t)g_scene.size();++i){
        const SceneObj& o=g_scene[i];SceneRuntime& r=g_sceneRuntime[i];
        r.world=objectWorld(o);
        if(o.mesh){
            float ex=(o.mesh->bmax.x-o.mesh->bmin.x)*o.scale*.5f;
            float ez=(o.mesh->bmax.z-o.mesh->bmin.z)*o.scale*.5f;
            r.radius=clampf(sqrtf(ex*ex+ez*ez),.45f,18.f);
            float baseY=groundH(o.x,o.z)+o.yOffset;
            r.y0=baseY+o.mesh->bmin.y*o.scale;r.y1=baseY+o.mesh->bmax.y*o.scale;
            if(r.y0>r.y1)std::swap(r.y0,r.y1);
        }
        r.cx=sceneCell(o.x);r.cz=sceneCell(o.z);
        g_sceneGrid[r.cz*SCENE_GRID_N+r.cx].push_back(i);
    }
    g_sceneRuntimeCount=g_scene.size();
}
static bool sceneRuntimeVisible(uint32_t i){
    const SceneObj& o=g_scene[i];const SceneRuntime& r=g_sceneRuntime[i];if(!o.mesh)return false;
    float tx=o.x-g_camTarget.x,tz=o.z-g_camTarget.z,dist2=tx*tx+tz*tz;if(dist2>8100.f)return false;
    float fdx=g_camTarget.x-g_camEye.x,fdz=g_camTarget.z-g_camEye.z;
    float odx=o.x-g_camEye.x,odz=o.z-g_camEye.z;if(fdx*odx+fdz*odz<-(r.radius*r.radius))return false;

    // Si el volumen del asset corta el segmento camara->jugador, no se dibuja: el jugador nunca queda tapado.
    float px=g_player.pos.x,pz=g_player.pos.z,py=g_player.pos.y+.85f;
    float sx=g_camEye.x-px,sz=g_camEye.z-pz,seg2=sx*sx+sz*sz;
    if(seg2>1e-5f){
        float ox=o.x-px,oz=o.z-pz,t=(ox*sx+oz*sz)/seg2;
        if(t>.02f&&t<.985f){
            float qx=px+sx*t,qz=pz+sz*t,dx=o.x-qx,dz=o.z-qz,rr=r.radius+.48f;
            if(dx*dx+dz*dz<rr*rr){float rayY=py+(g_camEye.y-py)*t;if(rayY>=r.y0-.3f&&rayY<=r.y1+.45f)return false;}
        }
    }
    return true;
}
static void drawVisibleScene(){
    rebuildSceneRuntime();
    // Solo visitar celdas que pueden entrar en el radio de render. Nada de recorrer todo g_scene cada frame.
    int ccx=sceneCell(g_camTarget.x),ccz=sceneCell(g_camTarget.z);const int reach=6;
    for(int z=(std::max)(0,ccz-reach);z<=(std::min)(SCENE_GRID_N-1,ccz+reach);++z){
        for(int x=(std::max)(0,ccx-reach);x<=(std::min)(SCENE_GRID_N-1,ccx+reach);++x){
            const auto& cell=g_sceneGrid[z*SCENE_GRID_N+x];
            for(uint32_t i:cell){if(!sceneRuntimeVisible(i))continue;const SceneObj& o=g_scene[i];drawMeshWorld(*o.mesh,g_sceneRuntime[i].world,o.tint,1,o.tex?o.tex:g_kaySRV);}
        }
    }
}

'@
$code=[regex]::Replace($code,$pattern,$runtime,1)

# Colision: consultar solo celdas vecinas al actor y usar radios cacheados.
$resolvePattern='(?s)static void resolveCharacter\(Character& c\)\{.*?\}(?=\s*static void updatePlayer)'
if(-not [regex]::IsMatch($code,$resolvePattern)){throw 'No encontre resolveCharacter.'}
$resolve=@'
static void resolveCharacter(Character& c){
    rebuildSceneRuntime();const float cr=.42f;int cx=sceneCell(c.pos.x),cz=sceneCell(c.pos.z);
    for(int gz=(std::max)(0,cz-2);gz<=(std::min)(SCENE_GRID_N-1,cz+2);++gz){for(int gx=(std::max)(0,cx-2);gx<=(std::min)(SCENE_GRID_N-1,cx+2);++gx){
        for(uint32_t i:g_sceneGrid[gz*SCENE_GRID_N+gx]){const SceneObj& o=g_scene[i];if(!o.collider||!o.mesh)continue;const SceneRuntime& rt=g_sceneRuntime[i];float dx=c.pos.x-o.x,dz=c.pos.z-o.z,r=(std::max)(.55f,rt.radius*.84f),ds=dx*dx+dz*dz,rr=(r+cr)*(r+cr);if(ds<rr&&ds>1e-6f){float d=sqrtf(ds),push=r+cr-d;c.pos.x+=dx/d*push;c.pos.z+=dz/d*push;}}
    }}c.pos.y=groundH(c.pos.x,c.pos.z);
}

'@
$code=[regex]::Replace($code,$resolvePattern,$resolve,1)

# Render estatico mediante rejilla + transforms cacheados. Reiniciar cache de state al comienzo del frame.
$oldLoop='for(const auto& o:g_scene)if(sceneObjectVisible(o))drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);'
if(-not $code.Contains($oldLoop)){throw 'No encontre loop de escena v16.'}
$code=$code.Replace($oldLoop,'drawVisibleScene();')
$oldRenderStart='if(!g_rtv)return; XMVECTOR frameEye=XMLoadFloat3(&g_camEye),frameAt=XMLoadFloat3(&g_camTarget),frameUp=XMVectorSet(0,1,0,0);'
$newRenderStart='if(!g_rtv)return; g_lastWorldVB=nullptr;g_lastWorldIB=nullptr;g_lastWorldSRV=nullptr; XMVECTOR frameEye=XMLoadFloat3(&g_camEye),frameAt=XMLoadFloat3(&g_camTarget),frameUp=XMVectorSet(0,1,0,0);'
if(-not $code.Contains($oldRenderStart)){throw 'No encontre inicio de render v16.'}
$code=$code.Replace($oldRenderStart,$newRenderStart)

# Mobs fuera del area visible no generan draw calls; su simulacion sigue activa.
$oldMobs='static void drawMobs(){for(const auto& mob:g_mobs){if(!mob.alive)continue;XMMATRIX actor=mobWorld(mob);for(const auto& sm:g_npc.meshes)drawMeshWorld(sm.gpu,actor,{1,1,1,.5f},1,g_npc.texture);}}'
$newMobs='static void drawMobs(){for(const auto& mob:g_mobs){if(!mob.alive)continue;float dx=mob.pos.x-g_camTarget.x,dz=mob.pos.z-g_camTarget.z;if(dx*dx+dz*dz>3600.f)continue;XMMATRIX actor=mobWorld(mob);for(const auto& sm:g_npc.meshes)drawMeshWorld(sm.gpu,actor,{1,1,1,.5f},1,g_npc.texture);}}'
if($code.Contains($oldMobs)){$code=$code.Replace($oldMobs,$newMobs)}

if($code -eq $original){throw 'El parche v17 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v17 aplicado: rejilla espacial, transforms/bounds cacheados, culling por celdas, oclusores ocultos, colision local y menos cambios de estado D3D.'
