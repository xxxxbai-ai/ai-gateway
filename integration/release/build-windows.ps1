param(
    [string]$QtRoot = 'C:/Qt/6.11.2/mingw_64',
    [string]$CompilerRoot = 'C:/Qt/Tools/mingw1310_64',
    [string]$CurlInclude = (Join-Path $env:USERPROFILE 'anaconda3/Library/include'),
    [string]$CurlLibrary = (Join-Path $env:USERPROFILE 'anaconda3/Library/lib/libcurl.lib'),
    [string]$NinjaPath = 'C:/Qt/Tools/Ninja/ninja.exe',
    [string]$BuildPath = (Join-Path $PSScriptRoot '../../build-delivery'),
    [switch]$GatewayOnly
)
$ErrorActionPreference = 'Stop'
foreach ($dependency in @((Join-Path $CompilerRoot 'bin/g++.exe'), $CurlLibrary, (Join-Path $CurlInclude 'curl/curl.h'), $NinjaPath)) {
    if (-not (Test-Path -LiteralPath $dependency -PathType Leaf)) { throw "缺少构建依赖：$dependency。可通过脚本参数指定实际路径；只运行交付程序无需编译。" }
}
$env:Path = (Join-Path $CompilerRoot 'bin') + ';' + (Join-Path $QtRoot 'bin') + ';' + $env:Path
$clientOption = if ($GatewayOnly) { 'OFF' } else { 'ON' }
& cmake -S $PSScriptRoot -B $BuildPath -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_CXX_COMPILER=$CompilerRoot/bin/g++.exe" "-DCMAKE_MAKE_PROGRAM=$NinjaPath" "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCURL_INCLUDE_DIR=$CurlInclude" "-DCURL_LIBRARY=$CurlLibrary" "-DBUILD_CLIENT=$clientOption"
if ($LASTEXITCODE -ne 0) { throw '构建配置失败。' }
& cmake --build $BuildPath --parallel 3
if ($LASTEXITCODE -ne 0) { throw '编译失败。' }
Write-Host "编译完成。测试前请将匹配的 libcurl 运行库放入 $BuildPath/bin；本机打包脚本会准备这些运行库。"
