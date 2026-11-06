$ErrorActionPreference='Stop'
$packageRoot=Split-Path -Parent $PSScriptRoot
try {
    $manifest=Get-Content -LiteralPath (Join-Path $packageRoot '交付清单.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach($entry in $manifest.files) {
        $resolved=[IO.Path]::GetFullPath((Join-Path $packageRoot $entry.path))
        $prefix=[IO.Path]::GetFullPath($packageRoot)+[IO.Path]::DirectorySeparatorChar
        if(-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw '交付清单包含无效路径。'}
        if(-not (Test-Path -LiteralPath $resolved -PathType Leaf)){throw "缺少文件：$($entry.path)"}
        $stream=[IO.File]::OpenRead($resolved)
        $hasher=[Security.Cryptography.SHA256]::Create()
        try {$actualHash=([BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}
        finally {$stream.Dispose();$hasher.Dispose()}
        if($actualHash -ne $entry.sha256){throw "文件与交付版本不一致：$($entry.path)"}
    }
    $gateway=Join-Path $packageRoot '网关/gateway.exe'
    & $gateway --check-config --config (Join-Path $packageRoot '网关/config/models.json')
    if($LASTEXITCODE -ne 0){throw '当前模型配置无效。'}
    Write-Host '文件和配置检查通过。' -ForegroundColor Green
    Write-Host "源码版本：$($manifest.source_commit)"
    Write-Host '本检查不发送聊天问题，不消耗模型额度。'
} catch {Write-Host ('检查失败：'+$_.Exception.Message) -ForegroundColor Red;exit 1}
