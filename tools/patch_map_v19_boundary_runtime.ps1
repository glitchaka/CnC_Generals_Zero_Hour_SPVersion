param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$old='    }}c.pos.y=groundH(c.pos.x,c.pos.z);'
$new='    }}c.pos.x=clampf(c.pos.x,-113.5f,113.5f);c.pos.z=clampf(c.pos.z,-113.5f,113.5f);c.pos.y=groundH(c.pos.x,c.pos.z);'
if(-not $code.Contains($old)){throw 'No encontre el cierre de resolveCharacter para restaurar el limite del mapa.'}
$code=$code.Replace($old,$new)
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v19 aplicado: limite fisico restaurado despues del runtime espacial.'
