param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}

# El build llama este parche inmediatamente despues de v12: aplicar aqui el pase final del mapa
# para que build_sp.cmd no pueda omitirlo aunque dist haya sido borrado.
& (Join-Path $PSScriptRoot 'patch_map_v13_final_map.ps1') -SourcePath $SourcePath
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}

$code=Get-Content -LiteralPath $SourcePath -Raw
$old='if(shield){int lh=findHandBone(c,false);XMMATRIX h=(lh>=0&&lh<(int)c.poseWorld.size())?c.poseWorld[lh]:XMMatrixTranslation(-.42f,.96f,.10f);XMMATRIX sh=XMMatrixScaling(.70f,.70f,.70f)*XMMatrixRotationY(XM_PIDIV2)*h*actor;drawMeshWorld(*shield,sh,{.48f,.58f,.72f,.5f},0,nullptr);}}'
$new='if(shield){int lb=-1;for(size_t i=0;i<c.bones.size();++i){std::string n=c.bones[i].name;std::transform(n.begin(),n.end(),n.begin(),[](unsigned char q){return (char)tolower(q);});bool left=n.find("left")!=std::string::npos||n.find(".l")!=std::string::npos||n.find("_l")!=std::string::npos;bool arm=n.find("forearm")!=std::string::npos||n.find("lowerarm")!=std::string::npos||n.find("lower_arm")!=std::string::npos;if(left&&arm){lb=(int)i;break;}}if(lb<0)lb=findHandBone(c,false);XMMATRIX h=(lb>=0&&lb<(int)c.poseWorld.size())?c.poseWorld[lb]:XMMatrixTranslation(-.42f,.96f,.10f);XMMATRIX sh=XMMatrixScaling(.60f,.60f,.60f)*XMMatrixRotationZ(-XM_PIDIV2)*XMMatrixRotationY(XM_PIDIV2)*XMMatrixTranslation(-.18f,.03f,.20f)*h*actor;drawMeshWorld(*shield,sh,{.48f,.58f,.72f,.5f},0,nullptr);}}'
if(-not $code.Contains($old)){throw 'No encontre el anclaje anterior del escudo.'}
$code=$code.Replace($old,$new)
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Escudo fijado al antebrazo exterior.'
