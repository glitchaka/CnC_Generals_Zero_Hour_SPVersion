param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

$needle='syncClientSize();updatePlayer(dt);'
$replacement='syncClientSize();if(g_gameState==GameState::Defeated){g_gameState=GameState::Playing;g_player.health=1.f;g_player.pos={-92.f,groundH(-92.f,-92.f),-92.f};g_player.home=g_player.pos;g_player.yaw=.78f;g_attackAnim=g_dodgeAnim=0.f;g_attackQueued=false;g_damageFlash=.65f;}updatePlayer(dt);'
if(-not $code.Contains($needle)){throw 'No encontre el inicio del bucle de actualizacion.'}
$code=$code.Replace($needle,$replacement)

# Elimina cualquier texto/boton de reinicio por derrota. La victoria puede conservar su reinicio.
$deathStart='if(g_gameState==GameState::Defeated){'
if($code.Contains($deathStart)){
  $victory='else if(g_gameState==GameState::Victory){'
  $a=$code.IndexOf($deathStart)
  $b=$code.IndexOf($victory,$a)
  if($a -ge 0 -and $b -gt $a){$code=$code.Remove($a,$b-$a).Insert($a,'if(false){')}
}

if($code -eq $original){throw 'El parche de respawn no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Respawn automatico fijado: muerte vuelve a base sin pantalla ni boton de reinicio.'
