<#
.SYNOPSIS
  Downloads ONNX Runtime and the ONNX models Vibe Hand needs.

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
    [switch]$SkipVoice,
    [switch]$WithInt8
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest is 10x faster without the progress bar
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$root = Split-Path -Parent $PSScriptRoot
$third = Join-Path $root 'third_party'
$ortDir = Join-Path $third 'onnxruntime'
$models = Join-Path $root 'models'
# Scratch space lives under third_party\ (git-ignored), not %TEMP%: with a
# non-ASCII user name %TEMP% is an 8.3 short path (C:\Users\84C4~1\...) that
# Remove-Item in Windows PowerShell 5.1 rejects, which failed the script and
# left ~0.5 GB behind on every run.
$tmp = Join-Path $third (".tmp-" + [guid]::NewGuid())
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
            # fp32 SFace: 15 ms per face on Zen 5 vs 110 ms for the int8 build, and more accurate.
            @{ f = 'face_recognition_sface_2021dec.onnx';          d = 'face_recognition_sface';        h = '0ba9fbfa01b5270c96627c4ef784da859931e02f04419c829e83484087c34e79' }
        )
        if ($WithInt8) {
            $list += @{ f = 'face_recognition_sface_2021dec_int8.onnx'; d = 'face_recognition_sface'; h = '2b0e941e6f16cc048c20aee0c8e31f569118f65d702914540f7bfdc14048d78a' }
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
    # ------------------------------------------------------------ voice commands
    # Vosk (offline speech recognition, Apache 2.0) + its small Russian model, and the
    # WeSpeaker ResNet34 speaker model ("is it the owner's voice"). ~90 MB download.
    if (-not $SkipVoice) {
        Write-Host "Voice commands" -ForegroundColor Cyan
        function Get-Verified($url, $out, $sha) {
            if ((Test-Path $out) -and ((Get-FileHash $out -Algorithm SHA256).Hash.ToLower() -eq $sha)) { return $false }
            Get-File $url $out
            $hash = (Get-FileHash $out -Algorithm SHA256).Hash.ToLower()
            if ($hash -ne $sha) { Remove-Item $out -Force; throw "SHA-256 mismatch for ${out}: got $hash" }
            return $true
        }
        $voskDir = Join-Path $third 'vosk'
        if (-not (Test-Path (Join-Path $voskDir 'libvosk.dll'))) {
            $zip = Join-Path $tmp 'vosk.zip'
            [void](Get-Verified 'https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-win64-0.3.45.zip' $zip 'f1dcc9cca460630f81ea8f71794f69c80bed6556d2a4e6237b5785e1d2dff34b')
            Expand-Archive $zip -DestinationPath (Join-Path $tmp 'vosk') -Force
            Copy-Found (Join-Path $tmp 'vosk') '*.dll' $voskDir
            Copy-Found (Join-Path $tmp 'vosk') 'vosk_api.h' $voskDir
        } else { Write-Host "  ok  vosk\libvosk.dll" }
        $ruDir = Join-Path $models 'vosk-model-small-ru'
        if (-not (Test-Path (Join-Path $ruDir 'am\final.mdl'))) {
            $zip = Join-Path $tmp 'vosk-ru.zip'
            [void](Get-Verified 'https://alphacephei.com/vosk/models/vosk-model-small-ru-0.22.zip' $zip '961d5ff98a17f4aa6de69864d0aa71fa5bac682301d2b5d17a3f24c5c99a46d4')
            Expand-Archive $zip -DestinationPath (Join-Path $tmp 'vosk-ru') -Force
            $src = Get-ChildItem (Join-Path $tmp 'vosk-ru') -Directory | Select-Object -First 1
            if (Test-Path $ruDir) { Remove-Item -Recurse -Force $ruDir }
            Copy-Item $src.FullName -Destination $ruDir -Recurse
        } else { Write-Host "  ok  vosk-model-small-ru" }
        $spk = Join-Path $models 'voice_speaker_resnet34.onnx'
        if (-not (Get-Verified 'https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-recongition-models/wespeaker_en_voxceleb_resnet34_LM.onnx' $spk 'e9848563da86f263117134dfd7ad63c92355b37de492b55e325400c9d9c39012')) {
            Write-Host "  ok  voice_speaker_resnet34.onnx"
        }
    }
    Write-Host "Done." -ForegroundColor Green
}
finally {
    try { [IO.Directory]::Delete($tmp, $true) } catch { Write-Warning "Could not remove $tmp : $_" }
}
