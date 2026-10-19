param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code

$needle='static void placeOriginalAsset(MeshGPU* m,const std::wstring& p,uint32_t h,int kind,ID3D11ShaderResourceView* tex){'
$insert='static void placeOriginalAsset(MeshGPU* m,const std::wstring& p,uint32_t h,int kind,ID3D11ShaderResourceView* tex){if(kind==2&&!g_boundaryWallMesh&&(p.find(L"\\wall.obj")!=std::wstring::npos||p.find(L"wall_arched.obj")!=std::wstring::npos||p.find(L"wall_half.obj")!=std::wstring::npos))g_boundaryWallMesh=m;'
if(-not $code.Contains($needle)){throw 'No encontre placeOriginalAsset.'}
$code=$code.Replace($needle,$insert)

# Limite fisico de respaldo: se inserta justo antes de recalcular Y, sin depender
# del texto exacto de las colisiones generado por los parches anteriores.
$groundExpr='c.pos.y=groundH(c.pos.x,c.pos.z);'
$clamped='c.pos.x=clampf(c.pos.x,-116.f,116.f);c.pos.z=clampf(c.pos.z,-116.f,116.f);c.pos.y=groundH(c.pos.x,c.pos.z);'
$bounded=[regex]::Replace($code,[regex]::Escape($groundExpr),$clamped,1)
if($bounded -eq $code){throw 'No encontre el recalculo de altura de resolveCharacter.'}
$code=$bounded

if($code -eq $original){throw 'El parche v9 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Limite del mapa corregido: muro visible y clamp fisico de respaldo.'
