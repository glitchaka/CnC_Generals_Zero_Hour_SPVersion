param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

# No cargar a VRAM cada OBJ de los cuatro packs completos. El mapa solo necesita un conjunto
# representativo de mallas y las piezas estrategicas. Las instancias reutilizan esas mallas.
$old='static void placeFullPack(const std::wstring& root,int kind,ID3D11ShaderResourceView* tex){std::vector<std::wstring> files;collectObjFiles(root,files);for(const auto& file:files){MeshGPU* m=new MeshGPU;if(!loadOBJ(file,*m)){delete m;continue;}g_worldOwned.push_back(m);std::wstring p=lowerW(file);placeOriginalAsset(m,p,mapHash(p),kind,tex);}}'
$new=@'
static bool runtimeAssetWanted(const std::wstring& p,int kind,size_t kept){
    if(kind==1){
        bool strategic=p.find(L"tower")!=std::wstring::npos||p.find(L"castle")!=std::wstring::npos||p.find(L"townhall")!=std::wstring::npos||p.find(L"building_")!=std::wstring::npos||p.find(L"wall")!=std::wstring::npos||p.find(L"gate")!=std::wstring::npos;
        bool lane=p.find(L"\\tiles\\roads\\")!=std::wstring::npos||p.find(L"hex_road_")!=std::wstring::npos||p.find(L"bridge")!=std::wstring::npos||p.find(L"\\tiles\\rivers\\")!=std::wstring::npos||p.find(L"hex_water")!=std::wstring::npos;
        return strategic||lane||kept<96;
    }
    if(kind==0){bool useful=p.find(L"tree")!=std::wstring::npos||p.find(L"bush")!=std::wstring::npos||p.find(L"grass")!=std::wstring::npos||p.find(L"flower")!=std::wstring::npos||p.find(L"rock")!=std::wstring::npos||p.find(L"stone")!=std::wstring::npos||p.find(L"cliff")!=std::wstring::npos||p.find(L"hill")!=std::wstring::npos;return useful&&kept<64;}
    if(kind==2){bool useful=p.find(L"wall")!=std::wstring::npos||p.find(L"column")!=std::wstring::npos||p.find(L"barrier")!=std::wstring::npos||p.find(L"rock")!=std::wstring::npos||p.find(L"torch")!=std::wstring::npos||p.find(L"chest")!=std::wstring::npos||p.find(L"crate")!=std::wstring::npos||p.find(L"barrel")!=std::wstring::npos;return useful&&kept<48;}
    if(kind==3){bool useful=p.find(L"sword")!=std::wstring::npos||p.find(L"shield")!=std::wstring::npos||p.find(L"bow")!=std::wstring::npos;return useful&&kept<12;}
    return false;
}
static void placeFullPack(const std::wstring& root,int kind,ID3D11ShaderResourceView* tex){std::vector<std::wstring> files;collectObjFiles(root,files);size_t kept=0;for(const auto& file:files){std::wstring p=lowerW(file);if(!runtimeAssetWanted(p,kind,kept))continue;MeshGPU* m=new MeshGPU;if(!loadOBJ(file,*m)){delete m;continue;}g_worldOwned.push_back(m);placeOriginalAsset(m,p,mapHash(p),kind,tex);++kept;}}
'@
RR $old $new 'carga selectiva de assets del mapa'

# El area jugable termina en +/-118. Generar suelo hasta 190 duplicaba mas de dos veces la
# geometria necesaria. Se deja margen suficiente fuera del muro sin construir terreno invisible.
RR 'const float S=1.55f;const float tileW=(maxX-minX)*S;const float tileH=(maxZ-minZ)*S;const float row=tileH*.75f;const float COVER=190.f;' 'const float S=1.55f;const float tileW=(maxX-minX)*S;const float tileH=(maxZ-minZ)*S;const float row=tileH*.75f;const float COVER=124.f;' 'cobertura del piso ajustada al mapa'

# La carga completa exigia artificialmente mas de 400 mallas residentes. Ya no debe hacerlo.
RR 'return g_worldOwned.size()>400&&g_laneTiles.size()>0;' 'return g_worldOwned.size()>40&&g_laneTiles.size()>0&&g_blueTowerMesh&&g_redTowerMesh&&g_blueCoreMesh&&g_redCoreMesh;' 'criterio de mapa cargado'

# Culling real para no enviar practicamente todo el mapa a la GPU cada frame.
$code=$code.Replace('if(dx*dx+dz*dz<15000.f)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);','if(dx*dx+dz*dz<9000.f)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);')

if($code -eq $original){throw 'El parche v14 no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Mapa v14 aplicado: menos mallas residentes, piso limitado al area real y culling mas estricto.'
