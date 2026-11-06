param([ValidateSet('normal','fallback','all-fail')][string]$Scenario='fallback',
      [int]$Port=8080,[int]$ProviderPort=18080)
$ErrorActionPreference='Stop'
$packageRoot=Split-Path -Parent $PSScriptRoot
$gateway=Join-Path $PSScriptRoot 'gateway.exe'
$provider=Join-Path $PSScriptRoot 'mock_provider.exe'
$ownedProvider=$null
try {
    $listeners=[System.Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()
    foreach($targetPort in @($Port,$ProviderPort)) {
        if($listeners | Where-Object {$_.Port -eq $targetPort}) {throw "端口 $targetPort 已被占用，请先关闭原来的网关或演示。"}
    }
    $config=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'config/models.demo.example.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $config.models=@($config.models | Where-Object {$_.is_mock})
    foreach($model in $config.models) {
        $model.enabled=$true
        $suffix=if($Scenario -eq 'all-fail' -or ($Scenario -eq 'fallback' -and $model.id -eq 'mock_code')) {'/scenario/reject503/v1'} else {'/v1'}
        $model.base_url="http://127.0.0.1:$ProviderPort$suffix"
    }
    $demoConfig=Join-Path $PSScriptRoot 'config/models.demo.runtime.json'
    [IO.File]::WriteAllText($demoConfig,($config | ConvertTo-Json -Depth 20),(New-Object Text.UTF8Encoding($false)))
    $logs=Join-Path $PSScriptRoot 'logs'
    [IO.Directory]::CreateDirectory($logs) | Out-Null
    $ownedProvider=Start-Process -FilePath $provider -ArgumentList @('--port',"$ProviderPort",'--delay-ms','1') -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $logs 'demo-provider.txt') -RedirectStandardError (Join-Path $logs 'demo-provider-errors.txt')
    $ready=$false
    for($i=0;$i -lt 40;$i++) {
        if($ownedProvider.HasExited){throw '模拟服务启动失败，请查看 logs/demo-provider-errors.txt。'}
        try {$health=Invoke-RestMethod -Uri "http://127.0.0.1:$ProviderPort/health" -TimeoutSec 1; $ready=$true;break} catch {Start-Sleep -Milliseconds 100}
    }
    if(-not $ready){throw '模拟服务未及时启动。'}
    Write-Host "模拟演示：$Scenario；不使用密钥，不调用外部模型。"
    Write-Host '客户端点击刷新模型，选择自动，输入“解释 C++ 指针”。手动选择模拟编程模型可演示失败时不切换。'
    & $gateway --port $Port --config $demoConfig --log (Join-Path $logs 'demo-requests.jsonl')
    if($LASTEXITCODE -ne 0){throw '演示网关退出异常。'}
} catch {Write-Host $_.Exception.Message -ForegroundColor Red;exit 1}
finally {if($null -ne $ownedProvider -and -not $ownedProvider.HasExited){$ownedProvider.Kill();$ownedProvider.WaitForExit()}}
