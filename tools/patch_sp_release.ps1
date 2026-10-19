param(
    [Parameter(Mandatory=$true)][string]$SourcePath
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $SourcePath)) { throw "No existe el fuente generado: $SourcePath" }

$code = Get-Content -LiteralPath $SourcePath -Raw
$original = $code

# Strip root translation from imported movement clips. Actor movement belongs to
# Character::pos; applying animated root translation as well makes the rendered
# mesh oscillate around its logical collider and looks like trembling.
$old = 'if(tr.path==AnimTrack::Translation)T[b]={v.x,v.y,v.z}; else if(tr.path==AnimTrack::Scale)S[b]={v.x,v.y,v.z}; else R[b]=v;'
$new = 'if(tr.path==AnimTrack::Translation){if(c.bones[b].parent>=0)T[b]={v.x,v.y,v.z};} else if(tr.path==AnimTrack::Scale)S[b]={v.x,v.y,v.z}; else R[b]=v;'
if (-not $code.Contains($old)) { throw 'No encontre el bloque de root motion esperado.' }
$code = $code.Replace($old,$new)

# Hysteresis at melee range prevents the NPC from alternating RUN/IDLE on
# consecutive frames when its distance hovers around the same threshold.
$old = 'g_npc.moving=d>1.28f;g_npc.speed=g_npc.moving?2.75f:0.f;'
$new = 'g_npc.moving=g_npc.moving?(d>1.20f):(d>1.38f);g_npc.speed=g_npc.moving?2.75f:0.f;'
if (-not $code.Contains($old)) { throw 'No encontre el umbral de movimiento NPC esperado.' }
$code = $code.Replace($old,$new)

# Resolve overlap deterministically by moving the NPC out of the player. The
# previous symmetric half-push fed back into the chase AI and caused contact jitter.
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

if ($code -eq $original) { throw 'El parche no produjo cambios.' }
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host "SP release source estabilizado: $SourcePath"
