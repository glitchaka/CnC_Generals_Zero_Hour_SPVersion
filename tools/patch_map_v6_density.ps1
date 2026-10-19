param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$original=$code
function RR([string]$a,[string]$b,[string]$n){if(-not $script:code.Contains($a)){throw "No encontre: $n"};$script:code=$script:code.Replace($a,$b)}

$old='static void scenicBelt(MeshGPU* m,uint32_t h,float target,ID3D11ShaderResourceView* tex){float ang=(h%6283)*.001f;float rad=126.f+(float)((h/17)%2400)/100.f;float x=cosf(ang)*rad,z=sinf(ang)*rad;addSceneMesh(m,x,z,-ang,target,false,tex,{.92f,.94f,.98f,1});}'
$new='static void scenicBelt(MeshGPU* m,uint32_t h,float target,ID3D11ShaderResourceView* tex){float ang=(h%6283)*.001f;float rad=121.f+(float)((h/17)%4400)/100.f;float wobble=((int)((h/97)%900)-450)/100.f;float x=cosf(ang)*rad+wobble,z=sinf(ang)*rad-wobble*.55f;addSceneMesh(m,x,z,-ang,target,false,tex,{.92f,.94f,.98f,1});}'
RR $old $new 'distribucion exterior de assets completos'

$old2='if(kind==0){target=cliff?5.5f:(tree?4.1f:(plant?1.1f:(rock?2.0f:1.5f)));coll=tree||cliff||rock;float r=coll?target*.42f:.15f;if(chooseJungleSlot(h,r,x,z)){addSceneMesh(m,x,z,yaw,target,coll,tex);return;}scenicBelt(m,h,target,tex);return;}'
$new2='if(kind==0){target=cliff?5.5f:(tree?4.1f:(plant?1.1f:(rock?2.0f:1.5f)));coll=tree||cliff||rock;float r=coll?target*.42f:.15f;bool active=(h%8u)==0u;if(active&&chooseJungleSlot(h,r,x,z)){addSceneMesh(m,x,z,yaw,target,coll,tex);return;}scenicBelt(m,h,target*.82f,tex);return;}'
RR $old2 $new2 'densidad legible de jungla'

$old3='for(const auto& o:g_scene)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);'
$new3='for(const auto& o:g_scene){float dx=o.x-g_camTarget.x,dz=o.z-g_camTarget.z;if(dx*dx+dz*dz<15000.f)drawMeshWorld(*o.mesh,objectWorld(o),o.tint,1,o.tex?o.tex:g_kaySRV);}'
RR $old3 $new3 'culling de escenario'

if($code -eq $original){throw 'El parche de densidad no produjo cambios.'}
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Densidad corregida: assets completos repartidos fuera del nucleo jugable y culling activo.'
