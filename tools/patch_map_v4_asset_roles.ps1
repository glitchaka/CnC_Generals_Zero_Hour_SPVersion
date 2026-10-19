param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
$old='bool road=p.find(L"road")!=std::wstring::npos||p.find(L"path")!=std::wstring::npos||p.find(L"tile")!=std::wstring::npos||p.find(L"bridge")!=std::wstring::npos;'
$new='bool road=p.find(L"\\tiles\\roads\\")!=std::wstring::npos||p.find(L"hex_road_")!=std::wstring::npos||p.find(L"building_bridge_")!=std::wstring::npos;'
if(-not $code.Contains($old)){throw 'No encontre clasificacion de caminos.'}
$code=$code.Replace($old,$new)
$old2='bool water=p.find(L"river")!=std::wstring::npos||p.find(L"water")!=std::wstring::npos;'
$new2='bool water=p.find(L"\\tiles\\rivers\\")!=std::wstring::npos||p.find(L"hex_water")!=std::wstring::npos;'
if(-not $code.Contains($old2)){throw 'No encontre clasificacion de rio.'}
$code=$code.Replace($old2,$new2)
if($code -eq $original){throw 'El ajuste de roles no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Roles de assets corregidos: roads solo en carriles y river tiles solo en rio.'
