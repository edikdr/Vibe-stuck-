<#
.SYNOPSIS
  Downloads ONNX Runtime and the ONNX models Vibe Legion needs.

.DESCRIPTION
  * ONNX Runtime -> third_party\onnxruntime\{include,lib}
      -Flavor cpu      : official CPU build (recommended: lowest power on Ryzen AI Max+)
      -Flavor directml : DirectML build for the Radeon 8060S iGPU (+ DirectML.dll)
  * Models -> models\  (OpenCV Zoo: MediaPipe palm/hand, YuNet, SFace), SHA-256 verified.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1
  powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1 -Flavor directml
#>
param(
    [ValidateSet('cpu', 'directml')] [string]$Flavor = 'cpu',
    [string]$OrtVersion = '1.23.2',
    [string]$OrtDirectMLVersion = '1.23.0',
    [string]$DirectMLVersion = '1.15.4',
    [switch]$SkipModels,
    [switch]$WithInt8
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest is 10x faster without the progress bar
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$root = Split-Path -Parent $PSScriptRoot
$third = Join-Path $root 'third_party'
$ortDir = Join-Path $third 'onnxruntime'
$models = Join-Path $root 'models'
$tmp = Join-Path $env:TEMP ("vibe-legion-" + [guid]::NewGuid())
New-Item -ItemType Directory -Force -Path $third, $models, $tmp | Out-Null

function Get-File($url, $out) {
    Write-Host "  -> $url"
    Invoke-WebRequest -Uri $url -OutFile $out -UseBasicParsing
}

function Copy-Found($from, $pattern, $to) {
    $files = Get-ChildItem -Path $from -Recurse -Filter $pattern -File
    if (-not $files) { throw "$pattern not found in package" }
    New-Item -ItemType Directory -Force -Path $to | Out-Null
    $files | ForEach-Object { Copy-Item $_.FullName -Destination $to -Force }
}

try {
    # ------------------------------------------------------------ ONNX Runtime
    Write-Host "ONNX Runtime ($Flavor)" -ForegroundColor Cyan
    if (Test-Path $ortDir) { Remove-Item -Recurse -Force $ortDir }
    if ($Flavor -eq 'cpu') {
        $zip = Join-Path $tmp 'ort.zip'
        Get-File "https://github.com/microsoft/onnxruntime/releases/download/v$OrtVersion/onnxruntime-win-x64-$OrtVersion.zip" $zip
        Expand-Archive $zip -DestinationPath $tmp -Force
        $src = Get-ChildItem $tmp -Directory -Filter 'onnxruntime-win-x64-*' | Select-Object -First 1
        Copy-Item (Join-Path $src.FullName 'include') -Destination (Join-Path $ortDir 'include') -Recurse
        Copy-Item (Join-Path $src.FullName 'lib') -Destination (Join-Path $ortDir 'lib') -Recurse
    } else {
        $pkg = Join-Path $tmp 'ort-dml.zip'
        Get-File "https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime.DirectML/$OrtDirectMLVersion" $pkg
        $ex = Join-Path $tmp 'ort-dml'
        Expand-Archive $pkg -DestinationPath $ex -Force
        Copy-Item (Join-Path $ex 'build\native\include') -Destination (Join-Path $ortDir 'include') -Recurse
        $native = Join-Path $ex 'runtimes\win-x64\native'
        Copy-Found $native '*.dll' (Join-Path $ortDir 'lib')
        Copy-Found $native '*.lib' (Join-Path $ortDir 'lib')

        $dml = Join-Path $tmp 'dml.zip'
        Get-File "https://www.nuget.org/api/v2/package/Microsoft.AI.DirectML/$DirectMLVersion" $dml
        $dex = Join-Path $tmp 'dml'
        Expand-Archive $dml -DestinationPath $dex -Force
        Copy-Item (Join-Path $dex 'bin\x64-win\DirectML.dll') -Destination (Join-Path $ortDir 'lib') -Force
        Write-Host "  Configure with: cmake --preset windows-directml" -ForegroundColor Yellow
    }

    # ------------------------------------------------------------ models
    if (-not $SkipModels) {
        Write-Host "Models" -ForegroundColor Cyan
        $base = 'https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models'
        $list = @(
            @{ f = 'palm_detection_mediapipe_2023feb.onnx';        d = 'palm_detection_mediapipe';      h = '78ff51c38496b7fc8b8ebdb6cc8c1abb02fa6c38427c6848254cdaba57fcce7c' },
            @{ f = 'handpose_estimation_mediapipe_2023feb.onnx';   d = 'handpose_estimation_mediapipe'; h = 'db0898ae717b76b075d9bf563af315b29562e11f8df5027a1ef07b02bef6d81c' },
            @{ f = 'face_detection_yunet_2023mar.onnx';            d = 'face_detection_yunet';          h = '8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4' },
            @{ f = 'face_recognition_sface_2021dec_int8.onnx';     d = 'face_recognition_sface';        h = '2b0e941e6f16cc048c20aee0c8e31f569118f65d702914540f7bfdc14048d78a' }
        )
        if ($WithInt8) {
            $list += @{ f = 'palm_detection_mediapipe_2023feb_int8.onnx';      d = 'palm_detection_mediapipe';      h = '9f014de96ef5b6816b3eb9a5fed21a7371ef0f104ea440aa19ce9129fe2af5f6' }
            $list += @{ f = 'handpose_estimation_mediapipe_2023feb_int8.onnx'; d = 'handpose_estimation_mediapipe'; h = 'e97bc1fb83b641954d33424c82b6ade719d0f73250bdb91710ecfd5f7b47e321' }
        }
        foreach ($m in $list) {
            $out = Join-Path $models $m.f
            if ((Test-Path $out) -and ((Get-FileHash $out -Algorithm SHA256).Hash.ToLower() -eq $m.h)) {
                Write-Host "  ok  $($m.f)"
                continue
            }
            Get-File "$base/$($m.d)/$($m.f)" $out
            $hash = (Get-FileHash $out -Algorithm SHA256).Hash.ToLower()
            if ($hash -ne $m.h) {
                Remove-Item $out -Force
                throw "SHA-256 mismatch for $($m.f): got $hash"
            }
        }
    }
    Write-Host "Done." -ForegroundColor Green
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
