param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\ExternalAssets\PolyHaven')
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$assets = @(
    @{ Id='forest_ground_04'; Kind='texture' },
    @{ Id='rocky_terrain_02'; Kind='texture' },
    @{ Id='mud_forest'; Kind='texture' },
    @{ Id='rock_09'; Kind='model' },
    @{ Id='rock_face_01'; Kind='model' },
    @{ Id='dead_quiver_trunk'; Kind='model' },
    @{ Id='wild_rooibos_bush'; Kind='model' },
    @{ Id='weed_plant_02'; Kind='model' }
)

function Flatten-Files($node, [string]$path='') {
    $out = New-Object System.Collections.Generic.List[object]
    if ($null -eq $node) { return $out }
    if ($node -is [System.Collections.IDictionary] -or $node.PSObject.Properties.Count -gt 0) {
        $props = @($node.PSObject.Properties)
        $hasUrl = $props.Name -contains 'url'
        if ($hasUrl -and $node.url) {
            $out.Add([pscustomobject]@{ Path=$path; Url=[string]$node.url; Size=$node.size })
        }
        foreach ($p in $props) {
            if ($p.Name -in @('url','size','md5')) { continue }
            $childPath = if ($path) { "$path/$($p.Name)" } else { $p.Name }
            foreach ($x in (Flatten-Files $p.Value $childPath)) { $out.Add($x) }
        }
    } elseif ($node -is [System.Collections.IEnumerable] -and -not ($node -is [string])) {
        $i=0
        foreach ($v in $node) {
            foreach ($x in (Flatten-Files $v "$path/$i")) { $out.Add($x) }
            $i++
        }
    }
    return $out
}

function Pick-TextureFile($files) {
    $cand = @($files | Where-Object {
        $_.Path -match '(?i)1k' -and $_.Path -match '(?i)(diff|diffuse|albedo)' -and $_.Url -match '(?i)\.(jpg|jpeg|png)(\?|$)'
    })
    if ($cand.Count -eq 0) {
        $cand = @($files | Where-Object { $_.Path -match '(?i)(diff|diffuse|albedo)' -and $_.Url -match '(?i)\.(jpg|jpeg|png)(\?|$)' })
    }
    return $cand | Sort-Object @{Expression={ if($_.Path -match '(?i)1k'){0}else{1} }}, @{Expression='Size'} | Select-Object -First 1
}

function Pick-ModelFile($files) {
    $cand = @($files | Where-Object {
        $_.Path -match '(?i)1k' -and $_.Path -match '(?i)(gltf|glb)' -and $_.Url -match '(?i)\.(gltf|glb|zip)(\?|$)'
    })
    if ($cand.Count -eq 0) {
        $cand = @($files | Where-Object { $_.Path -match '(?i)(gltf|glb)' -and $_.Url -match '(?i)\.(gltf|glb|zip)(\?|$)' })
    }
    if ($cand.Count -eq 0) {
        $cand = @($files | Where-Object { $_.Path -match '(?i)fbx' -and $_.Url -match '(?i)\.(fbx|zip)(\?|$)' })
    }
    return $cand | Sort-Object @{Expression={ if($_.Path -match '(?i)1k'){0}else{1} }}, @{Expression='Size'} | Select-Object -First 1
}

New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
$manifest = @()

foreach ($a in $assets) {
    Write-Host "[PolyHaven] $($a.Id)" -ForegroundColor Cyan
    $api = "https://api.polyhaven.com/files/$($a.Id)"
    $json = Invoke-RestMethod -Uri $api -Headers @{ 'User-Agent'='CnC-ZH-SPVersion/1.0 (Powered by Poly Haven)' }
    $flat = @(Flatten-Files $json)
    $pick = if ($a.Kind -eq 'texture') { Pick-TextureFile $flat } else { Pick-ModelFile $flat }
    if (-not $pick) { throw "No suitable downloadable file found for $($a.Id)" }

    $ext = [IO.Path]::GetExtension(([uri]$pick.Url).AbsolutePath)
    if (-not $ext) { $ext = '.bin' }
    $dir = Join-Path $OutputPath $a.Id
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $dest = Join-Path $dir ("asset" + $ext)

    if (-not (Test-Path $dest)) {
        Invoke-WebRequest -Uri $pick.Url -OutFile $dest -Headers @{ 'User-Agent'='CnC-ZH-SPVersion/1.0 (Powered by Poly Haven)' }
    }

    $manifest += [pscustomobject]@{ Id=$a.Id; Kind=$a.Kind; File=$dest; Source=$pick.Url; License='CC0'; Provider='Poly Haven' }
}

$manifest | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $OutputPath 'manifest.csv')
$manifest | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $OutputPath 'manifest.json')
@'
Poly Haven assets in this directory are CC0 / public domain.
Source: https://polyhaven.com
API integration credit: Powered by Poly Haven.
'@ | Set-Content -Encoding UTF8 (Join-Path $OutputPath 'LICENSE.txt')

Write-Host "CC0 assets ready in: $OutputPath" -ForegroundColor Green
