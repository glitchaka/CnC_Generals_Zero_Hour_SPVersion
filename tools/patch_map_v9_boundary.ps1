param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

$needle='static void placeOriginalAsset(MeshGPU* m,const std::wstring& p,uint32_t h,int kind,ID3D11ShaderResourceView* tex){'
$insert='static void placeOriginalAsset(MeshGPU* m,const std::wstring& p,uint32_t h,int kind,ID3D11ShaderResourceView* tex){if(kind==2&&!g_boundaryWallMesh&&(p.find(L"\\wall.obj")!=std::wstring::npos||p.find(L"wall_arched.obj")!=std::wstring::npos||p.find(L"wall_half.obj")!=std::wstring::npos))g_boundaryWallMesh=m;'
if(-not $code.Contains($needle)){throw 'No encontre placeOriginalAsset.'}
$code=$code.Replace($needle,$insert)

# El limite jugable no depende solo de que el jugador choque visualmente con el muro.
$old='} } c.pos.y=groundH(c.pos.x,c.pos.z);'
$new='} } c.pos.x=clampf(c.pos.x,-116.f,116.f);c.pos.z=clampf(c.pos.z,-116.f,116.f);c.pos.y=groundH(c.pos.x,c.pos.z);'
if(-not $code.Contains($old)){throw 'No encontre el cierre de resolveCharacter para limitar el mapa.'}
$code=$code.Replace($old,$new)

if($code -eq $original){throw 'El parche v9 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Limite del mapa corregido: wall.obj del Dungeon como muro visible y clamp fisico de respaldo.'
