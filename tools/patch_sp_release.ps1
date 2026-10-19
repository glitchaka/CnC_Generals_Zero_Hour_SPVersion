param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

function Replace-Required([string]$old,[string]$new,[string]$name){
  if(-not $script:code.Contains($old)){throw "No encontre: $name"}
  $script:code=$script:code.Replace($old,$new)
}

Replace-Required 'if(tr.path==AnimTrack::Translation)T[b]={v.x,v.y,v.z}; else if(tr.path==AnimTrack::Scale)S[b]={v.x,v.y,v.z}; else R[b]=v;' 'if(tr.path==AnimTrack::Translation){if(c.bones[b].parent>=0)T[b]={v.x,v.y,v.z};} else if(tr.path==AnimTrack::Scale)S[b]={v.x,v.y,v.z}; else R[b]=v;' 'root motion'
Replace-Required 'g_npc.moving=d>1.28f;g_npc.speed=g_npc.moving?2.75f:0.f;' 'g_npc.moving=g_npc.moving?(d>1.20f):(d>1.38f);g_npc.speed=g_npc.moving?2.75f:0.f;' 'histeresis NPC'

$old=@'
static void resolveCharacterPair(Character& a,Character& b){
    const float minD=.96f; float dx=a.pos.x-b.pos.x,dz=a.pos.z-b.pos.z,ds=dx*dx+dz*dz;
    if(ds>=minD*minD)return;
    if(ds<1e-8f){dx=1.f;dz=0.f;ds=1.f;}
    float d=sqrtf(ds),push=(minD-d)*.5f,nx=dx/d,nz=dz/d;
    a.pos.x+=nx*push;a.pos.z+=nz*push;b.pos.x-=nx*push;b.pos.z-=nz*push;
    resolveCharacter(a);resolveCharacter(b);
}
'@
$new=@'
static void resolveCharacterPair(Character& a,Character& b){
    const float minD=.96f; float dx=b.pos.x-a.pos.x,dz=b.pos.z-a.pos.z,ds=dx*dx+dz*dz;
    if(ds>=minD*minD)return;
    if(ds<1e-8f){dx=1.f;dz=0.f;ds=1.f;}
    float d=sqrtf(ds),push=minD-d,nx=dx/d,nz=dz/d;
    b.pos.x+=nx*push;b.pos.z+=nz*push;resolveCharacter(b);
}
'@
Replace-Required $old $new 'colision estable jugador-NPC'

Replace-Required 'static MeshGPU g_terrain,g_rubble,g_crates,g_barrel;' 'static MeshGPU g_terrain,g_rubble,g_crates,g_barrel,g_playerSword,g_npcSword,g_playerShield;' 'meshes de armas'

$old='if(!loadCharacter("ExternalAssets/KayKitAdventurers/Knight.glb",g_player)||!loadCharacter("ExternalAssets/KayKitAdventurers/Ranger.glb",g_npc)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\knight_texture.png",&g_player.texture)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\ranger_texture.png",&g_npc.texture)){MessageBoxW(g_hwnd,L"No pude cargar Knight/Ranger.",L"SPVersion",MB_ICONERROR);return 4;}'
$new='if(!loadOBJ(L"ExternalAssets\\KayKitAdventurers\\sword_1handed.obj",g_playerSword)||!loadOBJ(L"ExternalAssets\\KayKitAdventurers\\sword_2handed.obj",g_npcSword)||!loadOBJ(L"ExternalAssets\\KayKitAdventurers\\shield_round.obj",g_playerShield)){MessageBoxW(g_hwnd,L"Faltan las armas KayKit del demo.",L"SPVersion",MB_ICONERROR);return 4;} if(!loadCharacter("ExternalAssets/KayKitAdventurers/Knight.glb",g_player)||!loadCharacter("ExternalAssets/KayKitAdventurers/Ranger.glb",g_npc)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\knight_texture.png",&g_player.texture)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\ranger_texture.png",&g_npc.texture)){MessageBoxW(g_hwnd,L"No pude cargar Knight/Ranger.",L"SPVersion",MB_ICONERROR);return 5;}'
Replace-Required $old $new 'carga de armas/personajes'

Replace-Required 'float dist=12.5f*g_zoom' 'float dist=18.5f*g_zoom' 'distancia de camara'
Replace-Required 'g_zoom=clampf(g_zoom,.60f,1.65f)' 'g_zoom=clampf(g_zoom,.72f,1.80f)' 'rango de zoom'

Replace-Required 'float lit=.32+.62*sun+.18*sky;float3 col=base*lit;' 'float raw=.32+.62*sun+.18*sky;float lit=raw;if(color.a<.75){lit=raw>.82?1.06:(raw>.60?.82:(raw>.40?.60:.38));float3 v=normalize(cameraPos-i.wp);float rim=1-saturate(abs(dot(n,v)));lit*=lerp(1.0,.56,smoothstep(.50,.82,rim));}float3 col=base*lit;' 'cel shading'

$old='static void drawCharacter(const Character& c){ XMMATRIX actor=XMMatrixScaling(1.05f,1.05f,1.05f)*XMMatrixRotationY(c.yaw)*XMMatrixTranslation(c.pos.x,c.pos.y,c.pos.z); for(const auto& m:c.meshes)drawMeshWorld(m.gpu,actor,{1,1,1,1},1,c.texture); }'
$new=@'
static XMMATRIX actorWorld(const Character& c){return XMMatrixScaling(1.05f,1.05f,1.05f)*XMMatrixRotationY(c.yaw)*XMMatrixTranslation(c.pos.x,c.pos.y,c.pos.z);}
static void drawCharacter(const Character& c){XMMATRIX actor=actorWorld(c);for(const auto& m:c.meshes)drawMeshWorld(m.gpu,actor,{1,1,1,.5f},1,c.texture);}
static void drawEquipment(const Character& c,const MeshGPU& sword,const MeshGPU* shield,bool player){
    XMMATRIX actor=actorWorld(c);float swing=0.f;if(player&&g_attackCooldown>0.f){float p=1.f-clampf(g_attackCooldown/.48f,0.f,1.f);swing=-1.15f+p*2.25f;}
    XMMATRIX sw=XMMatrixScaling(.72f,.72f,.72f)*XMMatrixRotationX(XM_PIDIV2)*XMMatrixRotationZ(-.28f+swing)*XMMatrixTranslation(.43f,1.03f,.12f)*actor;drawMeshWorld(sword,sw,{.84f,.88f,.92f,.5f},0,nullptr);
    if(shield){XMMATRIX sh=XMMatrixScaling(.72f,.72f,.72f)*XMMatrixRotationY(XM_PIDIV2)*XMMatrixTranslation(-.42f,.96f,.10f)*actor;drawMeshWorld(*shield,sh,{.48f,.58f,.72f,.5f},0,nullptr);}
}
'@
Replace-Required $old $new 'render de equipo'
Replace-Required 'drawCharacter(g_npc);drawCharacter(g_player);drawHUD();g_swap->Present(1,0);' 'drawCharacter(g_npc);drawEquipment(g_npc,g_npcSword,nullptr,false);drawCharacter(g_player);drawEquipment(g_player,g_playerSword,&g_playerShield,true);drawHUD();g_swap->Present(1,0);' 'orden de render'

$oldHud=@'
static void drawHUD(){
    float cx=g_w*.5f;drawUIQuad(g_uiPanel,cx-270.f,g_h-116.f,540.f,92.f,{1,1,1,.96f});drawUIQuad(g_uiHealth,cx-240.f,g_h-94.f,210.f*g_player.health,28.f,{1,1,1,1});
    for(int i=0;i<5;++i)drawUIQuad(i==0?g_uiSlot:g_uiPanel,cx+4.f+i*52.f,g_h-96.f,46.f,46.f,{1,1,1,1});
    float ready=1.f-clampf(g_attackCooldown/.48f,0.f,1.f);drawUIQuad(g_uiWhite,cx+8.f,g_h-91.f,38.f,38.f*ready,{.95f,.95f,1.f,.72f});
    float dash=1.f-clampf(g_dashCooldown/1.15f,0.f,1.f);drawUIQuad(g_uiWhite,cx+60.f,g_h-91.f,38.f,38.f*dash,{.72f,.88f,1.f,.68f});
    drawUIQuad(g_uiPanel,cx-190.f,28.f,380.f,38.f,{1,1,1,.9f});drawUIQuad(g_uiWhite,cx-178.f,39.f,356.f*g_enemyHealth,16.f,{.88f,.20f,.16f,.95f});
    drawUIQuad(g_uiWhite,cx-7.f,g_h*.5f-1.f,14.f,2.f,{1,1,1,.72f});drawUIQuad(g_uiWhite,cx-1.f,g_h*.5f-7.f,2.f,14.f,{1,1,1,.72f});
    if(g_enemyHitFlash>0.f)drawUIQuad(g_uiWhite,cx-196.f,24.f,392.f,46.f,{1.f,.82f,.35f,g_enemyHitFlash});
    if(g_damageFlash>0.f)drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.95f,.05f,.03f,g_damageFlash*.32f});
    if(g_gameState!=GameState::Playing){XMFLOAT4 c=g_gameState==GameState::Victory?XMFLOAT4{.18f,.82f,.38f,.84f}:XMFLOAT4{.82f,.12f,.10f,.84f};drawUIQuad(g_uiPanel,cx-260.f,g_h*.5f-82.f,520.f,164.f,{1,1,1,.96f});drawUIQuad(g_uiWhite,cx-220.f,g_h*.5f-30.f,440.f,60.f,c);drawUIQuad(g_uiWhite,cx-95.f,g_h*.5f+48.f,190.f,5.f,{1,1,1,.8f});}
}
'@
$newHud=@'
static const char* glyph5x7(char c){switch(c){
case 'A':return "01110100011000111111100011000110001";case 'C':return "01111100001000010000100001000001111";case 'D':return "11110100011000110001100011000111110";case 'E':return "11111100001000011110100001000011111";case 'G':return "01111100001000010111100011000101111";case 'H':return "10001100011000111111100011000110001";case 'I':return "11111001000010000100001000010011111";case 'M':return "10001110111010110101100011000110001";case 'N':return "10001110011010110011100011000110001";case 'O':return "01110100011000110001100011000101110";case 'Q':return "01110100011000110001101011001001101";case 'R':return "11110100011000111110101001001010001";case 'S':return "01111100001000001110000010000111110";case 'T':return "11111001000010000100001000010000100";case 'U':return "10001100011000110001100011000101110";case 'V':return "10001100011000110001100010101000100";case 'Y':return "10001100010101000100001000010000100";case '-':return "00000000000000011111000000000000000";default:return "00000000000000000000000000000000000";}}
static void drawText5x7(const char* s,float x,float y,float z,const XMFLOAT4& col){float ox=x;for(;*s;++s){if(*s==' '){x+=6*z;continue;}if(*s=='\n'){y+=9*z;x=ox;continue;}const char* g=glyph5x7(*s);for(int yy=0;yy<7;++yy)for(int xx=0;xx<5;++xx)if(g[yy*5+xx]=='1')drawUIQuad(g_uiWhite,x+xx*z,y+yy*z,z,z,col);x+=6*z;}}
static void drawHUD(){
    float cx=g_w*.5f;drawUIQuad(g_uiPanel,24.f,g_h-112.f,330.f,82.f,{1,1,1,.94f});drawText5x7("VIDA",44.f,g_h-94.f,3.f,{1,1,1,.95f});drawUIQuad(g_uiWhite,122.f,g_h-91.f,205.f,18.f,{.12f,.12f,.14f,.82f});drawUIQuad(g_uiHealth,124.f,g_h-89.f,201.f*g_player.health,14.f,{1,1,1,1});drawText5x7("RMB ATAQUE",44.f,g_h-62.f,2.f,{.92f,.94f,1.f,.94f});drawText5x7("SPACE ESQUIVA",184.f,g_h-62.f,2.f,{.92f,.94f,1.f,.94f});
    drawUIQuad(g_uiPanel,cx-230.f,24.f,460.f,62.f,{1,1,1,.92f});drawText5x7("ENEMIGO",cx-62.f,34.f,2.f,{1,1,1,.96f});drawUIQuad(g_uiWhite,cx-198.f,60.f,396.f,13.f,{.12f,.12f,.14f,.82f});drawUIQuad(g_uiWhite,cx-196.f,62.f,392.f*g_enemyHealth,9.f,{.86f,.18f,.14f,.98f});
    if(g_damageFlash>0.f)drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.95f,.04f,.03f,g_damageFlash*.30f});
    if(g_gameState==GameState::Defeated){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.04f,.02f,.02f,.52f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("HAS MUERTO",cx-135.f,g_h*.5f-48.f,4.f,{.95f,.18f,.14f,1});drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});}
    else if(g_gameState==GameState::Victory){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.02f,.06f,.03f,.40f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("VICTORIA",cx-108.f,g_h*.5f-48.f,4.f,{.25f,.92f,.42f,1});drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});}
}
'@
Replace-Required $oldHud $newHud 'HUD profesional'

Replace-Required "case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='R'&&g_gameState!=GameState::Playing)resetGame();if(w==VK_ESCAPE)DestroyWindow(h);return 0;" "case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='R'||((w==VK_RETURN)&&g_gameState!=GameState::Playing))resetGame();if(w==VK_ESCAPE)DestroyWindow(h);return 0;" 'reinicio'
Replace-Required 'killCharacter(g_npc);killCharacter(g_player);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);' 'killCharacter(g_npc);killCharacter(g_player);killMesh(g_playerShield);killMesh(g_npcSword);killMesh(g_playerSword);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);' 'limpieza armas'

if($code -eq $original){throw 'El parche no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host "SP release source estabilizado y presentado: $SourcePath"
