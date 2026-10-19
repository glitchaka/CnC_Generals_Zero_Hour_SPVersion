param([Parameter(Mandatory=$true)][string]$SourcePath)
$ErrorActionPreference='Stop'
if(-not(Test-Path -LiteralPath $SourcePath)){throw "No existe el fuente generado: $SourcePath"}
$code=Get-Content -LiteralPath $SourcePath -Raw
$old='static void scenicBelt(MeshGPU* m,uint32_t h,float target,ID3D11ShaderResourceView* tex){float ang=(h%6283)*.001f;float rad=126.f+(float)((h/17)%2400)/100.f;float x=cosf(ang)*rad,z=sinf(ang)*rad;addSceneMesh(m,x,z,-ang,target,false,tex,{.92f,.94f,.98f,1});}'
$new=@'
static void scenicBelt(MeshGPU* m,uint32_t h,float target,ID3D11ShaderResourceView* tex){
    // Todos los modelos que no cumplen una funcion jugable siguen presentes, pero ya no se
    // amontonan. Se reparten en cuatro franjas exteriores sobre la misma reticula de piso.
    static uint32_t seq=0;uint32_t id=seq++;uint32_t side=id&3u;uint32_t slot=id>>2;
    const float STEP=5.0f;const int ALONG=67;float along=-165.f+(slot%ALONG)*STEP;float depth=126.f+((slot/ALONG)%8)*STEP;
    float x=0,z=0,yaw=0;if(side==0){x=along;z=-depth;yaw=0;}else if(side==1){x=depth;z=along;yaw=-XM_PIDIV2;}else if(side==2){x=-along;z=depth;yaw=XM_PI;}else{x=-depth;z=-along;yaw=XM_PIDIV2;}
    float jitter=((int)(h%101)-50)*.006f;addSceneMesh(m,x+jitter,z-jitter,yaw,target,false,tex,{.92f,.94f,.98f,1});
}
'@
if(-not $code.Contains($old)){throw 'No encontre el distribuidor exterior anterior.'}
$code=$code.Replace($old,$new)
Set-Content -LiteralPath $SourcePath -Value $code -Encoding UTF8 -NoNewline
Write-Host 'Distribucion v11 aplicada: assets completos repartidos en franjas exteriores sin apilamiento.'
