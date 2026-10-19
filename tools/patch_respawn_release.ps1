param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

$needle='syncClientSize();updatePlayer(dt);'
$replacement='syncClientSize();if(g_gameState==GameState::Defeated){g_gameState=GameState::Playing;g_player.health=1.f;g_player.pos={-92.f,groundH(-92.f,-92.f),-92.f};g_player.home=g_player.pos;g_player.yaw=.78f;g_attackAnim=g_dodgeAnim=0.f;g_attackQueued=false;g_damageFlash=.65f;}updatePlayer(dt);'
if(-not $code.Contains($needle)){throw 'No encontre el inicio del bucle de actualizacion.'}
$code=$code.Replace($needle,$replacement)

# Quita solo el overlay de derrota. No corta rangos arbitrarios del fuente ni deja un if mal formado.
$deathHud='if(g_gameState==GameState::Defeated){drawUIQuad(g_uiWhite,0,0,(float)g_w,(float)g_h,{.04f,.02f,.02f,.52f});drawUIQuad(g_uiPanel,cx-250.f,g_h*.5f-88.f,500.f,176.f,{1,1,1,.96f});drawText5x7("HAS MUERTO",cx-135.f,g_h*.5f-48.f,4.f,{.95f,.18f,.14f,1});drawText5x7("R - REINICIAR",cx-115.f,g_h*.5f+28.f,3.f,{1,1,1,.96f});}'
if($code.Contains($deathHud)){$code=$code.Replace($deathHud,'')}
$code=$code.Replace('else if(g_gameState==GameState::Victory){','if(g_gameState==GameState::Victory){')

if($code -eq $original){throw 'El parche de respawn no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Respawn automatico fijado: muerte vuelve a base sin pantalla ni boton de reinicio.'
