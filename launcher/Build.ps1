param([string]$DevkitPro='C:\devkitPro')
$ErrorActionPreference='Stop'
$bash=Join-Path $DevkitPro 'msys2/usr/bin/bash.exe'
if(!(Test-Path -LiteralPath $bash)) { throw 'devkitPro MSYS2 bash was not found. Set -DevkitPro to its installation folder.' }
$curlHeader=Join-Path $DevkitPro 'portlibs/wiiu/include/curl/curl.h'
if(!(Test-Path -LiteralPath $curlHeader)) { throw 'Install devkitPro portlib wiiu-curl (dkp-pacman -S wiiu-curl) before building.' }
Push-Location $PSScriptRoot
try {
    & $bash -c 'make -C ../dependencies/libgui -j4 && make -C ../dependencies/librpxloader -j4 && make -C ../dependencies/libwuhbutils -j4 && make -j4'
    if($LASTEXITCODE -ne 0) { throw "Compiler failed with exit code $LASTEXITCODE" }
    Write-Host "Built $(Join-Path $PSScriptRoot 'men.rpx')"
} finally { Pop-Location }
