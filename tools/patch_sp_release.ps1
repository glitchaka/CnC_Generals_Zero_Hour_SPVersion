param(
    [Parameter(Mandatory=$true)][string]$SourcePath
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $SourcePath)) { throw "No existe el fuente generado: $SourcePath" }

$code = Get-Content -LiteralPath $SourcePath -Raw
$original = $code

$old = 'if(tr.path==AnimTrack::Translation)T[b]={v.x,v.y,v.z}; else if(tr.path==AnimTrack::Scale)S[b]={v.x,v.y,v.z}; else R[b]=v;'
$new = 'if(tr.path==AnimTrack::Translation){if(c.bones[b].parent>=0)T[b]={v.x,v.y,v.z};} else if(tr.path==AnimTrack::Scale)S[b]={v.x,v.y,v.z}; else R[b]=v;'
if (-not $code.Contains($old)) { throw 'No encontre el bloque de root motion esperado.' }
$code = $code.Replace($old,$new)

$old = 'g_npc.moving=d>1.28f;g_npc.speed=g_npc.moving?2.75f:0.f;'
$new = 'g_npc.moving=g_npc.moving?(d>1.20f):(d>1.38f);g_npc.speed=g_npc.moving?2.75f:0.f;'
if (-not $code.Contains($old)) { throw 'No encontre el umbral de movimiento NPC esperado.' }
$code = $code.Replace($old,$new)

$old = @'
static void resolveCharacterPair(Character& a,Character& b){
    const float minD=.96f; float dx=a.pos.x-b.pos.x,dz=a.pos.z-b.pos.z,ds=dx*dx+dz*dz;
    if(ds>=minD*minD)return;
    if(ds<1e-8f){dx=1.f;dz=0.f;ds=1.f;}
    float d=sqrtf(ds),push=(minD-d)*.5f,nx=dx/d,nz=dz/d;
    a.pos.x+=nx*push;a.pos.z+=nz*push;b.pos.x-=nx*push;b.pos.z-=nz*push;
    resolveCharacter(a);resolveCharacter(b);
}
'@
$new = @'
static void resolveCharacterPair(Character& a,Character& b){
    const float minD=.96f; float dx=b.pos.x-a.pos.x,dz=b.pos.z-a.pos.z,ds=dx*dx+dz*dz;
    if(ds>=minD*minD)return;
    if(ds<1e-8f){dx=1.f;dz=0.f;ds=1.f;}
    float d=sqrtf(ds),push=minD-d,nx=dx/d,nz=dz/d;
    b.pos.x+=nx*push;b.pos.z+=nz*push;
    resolveCharacter(b);
}
'@
if (-not $code.Contains($old)) { throw 'No encontre resolveCharacterPair esperado.' }
$code = $code.Replace($old,$new)

$old = 'static MeshGPU g_terrain,g_rubble,g_crates,g_barrel;'
$new = 'static MeshGPU g_terrain,g_rubble,g_crates,g_barrel,g_playerSword,g_npcSword,g_playerShield;'
if (-not $code.Contains($old)) { throw 'No encontre los meshes globales.' }
$code = $code.Replace($old,$new)

$old = 'if(!loadCharacter("ExternalAssets/KayKitAdventurers/Knight.glb",g_player)||!loadCharacter("ExternalAssets/KayKitAdventurers/Ranger.glb",g_npc)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\knight_texture.png",&g_player.texture)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\ranger_texture.png",&g_npc.texture)){MessageBoxW(g_hwnd,L"No pude cargar Knight/Ranger.",L"SPVersion",MB_ICONERROR);return 4;}'
$new = 'if(!loadOBJ(L"ExternalAssets\\KayKitAdventurers\\sword_1handed.obj",g_playerSword)||!loadOBJ(L"ExternalAssets\\KayKitAdventurers\\sword_2handed.obj",g_npcSword)||!loadOBJ(L"ExternalAssets\\KayKitAdventurers\\shield_round.obj",g_playerShield)){MessageBoxW(g_hwnd,L"Faltan las armas KayKit del demo.",L"SPVersion",MB_ICONERROR);return 4;} if(!loadCharacter("ExternalAssets/KayKitAdventurers/Knight.glb",g_player)||!loadCharacter("ExternalAssets/KayKitAdventurers/Ranger.glb",g_npc)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\knight_texture.png",&g_player.texture)||!loadTextureWIC(L"ExternalAssets\\KayKitAdventurers\\ranger_texture.png",&g_npc.texture)){MessageBoxW(g_hwnd,L"No pude cargar Knight/Ranger.",L"SPVersion",MB_ICONERROR);return 5;}'
if (-not $code.Contains($old)) { throw 'No encontre carga de personajes esperada.' }
$code = $code.Replace($old,$new)

$code = $code.Replace('float dist=12.5f*g_zoom','float dist=18.5f*g_zoom')
$code = $code.Replace('g_zoom=clampf(g_zoom,.60f,1.65f)','g_zoom=clampf(g_zoom,.72f,1.80f)')

$old = 'float lit=.32+.62*sun+.18*sky;float3 col=base*lit;'
$new = 'float raw=.32+.62*sun+.18*sky;float lit=raw;if(color.a<.75){lit=raw>.82?1.06:(raw>.60?.82:(raw>.40?.60:.38));float3 v=normalize(cameraPos-i.wp);float rim=1-saturate(abs(dot(n,v)));lit*=lerp(1.0,.56,smoothstep(.50,.82,rim));}float3 col=base*lit;'
if (-not $code.Contains($old)) { throw 'No encontre el shader de iluminacion esperado.' }
$code = $code.Replace($old,$new)

$old = 'static void drawCharacter(const Character& c){ XMMATRIX actor=XMMatrixScaling(1.05f,1.05f,1.05f)*XMMatrixRotationY(c.yaw)*XMMatrixTranslation(c.pos.x,c.pos.y,c.pos.z); for(const auto& m:c.meshes)drawMeshWorld(m.gpu,actor,{1,1,1,1},1,c.texture); }'
$new = @'
static XMMATRIX actorWorld(const Character& c){return XMMatrixScaling(1.05f,1.05f,1.05f)*XMMatrixRotationY(c.yaw)*XMMatrixTranslation(c.pos.x,c.pos.y,c.pos.z);}
static void drawCharacter(const Character& c){XMMATRIX actor=actorWorld(c);for(const auto& m:c.meshes)drawMeshWorld(m.gpu,actor,{1,1,1,.5f},1,c.texture);}
static void drawEquipment(const Character& c,const MeshGPU& sword,const MeshGPU* shield,bool player){
    XMMATRIX actor=actorWorld(c);float swing=0.f;if(player&&g_attackCooldown>0.f){float p=1.f-clampf(g_attackCooldown/.48f,0.f,1.f);swing=-1.15f+p*2.25f;}
    XMMATRIX sw=XMMatrixScaling(.72f,.72f,.72f)*XMMatrixRotationX(XM_PIDIV2)*XMMatrixRotationZ(-.28f+swing)*XMMatrixTranslation(.43f,1.03f,.12f)*actor;
    drawMeshWorld(sword,sw,{.84f,.88f,.92f,.5f},0,nullptr);
    if(shield){XMMATRIX sh=XMMatrixScaling(.72f,.72f,.72f)*XMMatrixRotationY(XM_PIDIV2)*XMMatrixTranslation(-.42f,.96f,.10f)*actor;drawMeshWorld(*shield,sh,{.48f,.58f,.72f,.5f},0,nullptr);}
}
'@
if (-not $code.Contains($old)) { throw 'No encontre drawCharacter esperado.' }
$code = $code.Replace($old,$new)

$old = 'drawCharacter(g_npc);drawCharacter(g_player);drawHUD();g_swap->Present(1,0);'
$new = 'drawCharacter(g_npc);drawEquipment(g_npc,g_npcSword,nullptr,false);drawCharacter(g_player);drawEquipment(g_player,g_playerSword,&g_playerShield,true);drawHUD();g_swap->Present(1,0);'
if (-not $code.Contains($old)) { throw 'No encontre orden de render de personajes.' }
$code = $code.Replace($old,$new)

$hudPattern = '(?s)static void drawHUD\(\)\{.*?\n\}\n\nstatic void render\(\)'
if (-not [regex]::IsMatch($code,$hudPattern)) { throw 'No encontre drawHUD generado.' }
$hud = @'
static const char* glyph5x7(char c){
    switch(c){
      case 'A':return "01110100011000111111100011000110001";case 'B':return "11110100011000111110100011000111110";case 'C':return "01111100001000010000100001000001111";
      case 'D':return "11110100011000110001100011000111110";case 'E':return "11111100001000011110100001000011111";case 'G':return "01111100001000010111100011000101111";
      case 'H':return "10001100011000111111100011000110001";case 'I':return "11111001000010000100001000010011111";case 'M':return "10001110111010110101100011000110001";
      case 'N':return "10001110011010110011100011000110001";case 'O':return "01110100011000110001100011000101110";case 'P':return "11110100011000111110100001000010000";
      case 'Q':return "01110100011000110001101011001001101";case 'R':return "11110100011000111110101001001010001";case 'S':return "01111100001000001110000010000111110";
      case 'T':return "11111001000010000100001000010000100";case 'U':return "10001100011000110001100011000101110";case 'V':return "10001100011000110001100010101000100";
      case 'Y':return "10001100010101000100001000010000100";case '0':return "01110100011001110101110011000101110";case '1':return "00100011000010000100001000010001110";
      case '2':return "01110100010000100110010001000011111";case '3':return "11110000010000101110000010000111110";case '-':return "00000000000000011111000000000000000";
      case ':':return "00000001000010000000001000010000000";default:return "00000000000000000000000000000000000";
    }
}
static void drawText5x7(const char* s,float x,float y,float scale,const XMFLOAT4& col){
    float ox=x;for(;*s;++s){char c=*s;if(c==' '){x+=6*scale;continue;}if(c=='\n'){y+=9*scale;x=ox;continue;}const char* g=glyph5x7(c);for(int yy=0;yy<7;++yy)for(int xx=0;xx<5;++xx)if(g[yy*5+xx]=='1')drawUIQuad(g_uiWhite,x+xx*scale,y+yy*scale,scale,scale,col);x+=6*scale;}
}
static void drawHUD(){
    float cx=g_w*.5f;
    drawUIQuad(g_uiPanel,24.f,g_h-112.f,320.f,82.f,{1,1,1,.94f});
    drawText5x7("VIDA",44.f,g_h-94.f,3.f,{1,1,1,.95f});drawUIQuad(g_uiWhite,122.f,g_h-91.f,196.f,18.f,{.12f,.12f,.14f,.82f});drawUIQuad(g_uiHealth,124.f,g_h-89.f,192.f*g_player.health,14.f,{1,1,1,1});
    drawText5x7("RMB ATAQUE",44.f,g_h-62.f,2.f,{.92f,.94f,1.f,.94f});drawText5x7("SPACE ESQUIVA",182.f,g_h-62.f,2.f,{.92f,.94f,1.f,.94f});
    drawUIQuad(g_uiPanel,cx-230.f,24.f,460.f,62.f,{1,1,1,.92f});drawText5x7("ENEMIGO",cx-62.f,34.f,2.f,{1,1,1,.96f});drawUIQuad(g_uiWhite,cx-198.f,60.f,396.f,13.f,{.12f,.12f,.14f,.82f});drawUIQuad(g_uiWhite,cx-196.f,62.f,392.f*g_enemyHealth,9.f,{.86f,.18f,.14f,.98f});
    if(g_damageFlash>0.f)drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.95f,.04f,.03f,g_damageFlash*.30f});
    if(g_gameState==GameState::Defeated){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.04f,.02f,.02f,.52f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("HAS MUERTO",cx-135.f,g_h*.5f-48.f,4.f,{.95f,.18f,.14f,1});drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});}
    else if(g_gameState==GameState::Victory){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.02f,.06f,.03f,.40f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("VICTORIA",cx-108.f,g_h*.5f-48.f,4.f,{.25f,.92f,.42f,1});drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});}
}

static void render()
'@
$code = [regex]::Replace($code,$hudPattern,$hud,1)

$old = "case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='R'&&g_gameState!=GameState::Playing)resetGame();if(w==VK_ESCAPE)DestroyWindow(h);return 0;"
$new = "case WM_KEYDOWN:if(w<256)g_keys[w]=true;if(w=='R'||((w==VK_RETURN)&&g_gameState!=GameState::Playing))resetGame();if(w==VK_ESCAPE)DestroyWindow(h);return 0;"
if (-not $code.Contains($old)) { throw 'No encontre WM_KEYDOWN de reinicio.' }
$code = $code.Replace($old,$new)

$old = 'killCharacter(g_npc);killCharacter(g_player);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);'
$new = 'killCharacter(g_npc);killCharacter(g_player);killMesh(g_playerShield);killMesh(g_npcSword);killMesh(g_playerSword);killMesh(g_barrel);killMesh(g_crates);killMesh(g_rubble);killMesh(g_terrain);'
if (-not $code.Contains($old)) { throw 'No encontre limpieza de meshes.' }
$code = $code.Replace($old,$new)

if ($code -eq $original) { throw 'El parche no produjo cambios.' }
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host "SP release source estabilizado y presentado: $SourcePath"
