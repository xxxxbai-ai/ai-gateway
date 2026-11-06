param(
    [string]$GatewayPath = (Join-Path $PSScriptRoot 'gateway.exe'),
    [string]$ConfigPath = (Join-Path $PSScriptRoot 'config/models.deepseek.json'),
    [int]$Port = 8080
)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
try {
    if (-not (Test-Path -LiteralPath $GatewayPath -PathType Leaf)) { throw '找不到网关程序，请使用完整的真实网关运行包。' }
    if (-not (Test-Path -LiteralPath $ConfigPath -PathType Leaf)) { throw '找不到模型配置文件。' }
    $listeners = [System.Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()
    if ($listeners | Where-Object { $_.Port -eq $Port }) {
        throw "端口 $Port 已被占用。请先在之前的网关窗口按 Ctrl+C 退出，再启动本窗口。"
    }
    if ([string]::IsNullOrWhiteSpace($env:DEEPSEEK_API_KEY)) {
        $userCredential = [Environment]::GetEnvironmentVariable('DEEPSEEK_API_KEY', 'User')
        if (-not [string]::IsNullOrWhiteSpace($userCredential)) {
            $env:DEEPSEEK_API_KEY = $userCredential
            $userCredential = $null
        } else {
            Write-Host '请输入本机使用的 DeepSeek API 密钥。输入不显示明文，也不会保存到文件。'
            $secureCredential = Read-Host 'DeepSeek API Key' -AsSecureString
            $credentialPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secureCredential)
            try { $env:DEEPSEEK_API_KEY = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($credentialPointer).Trim() }
            finally {
                [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($credentialPointer)
                $secureCredential.Dispose()
            }
        }
    }
    if ([string]::IsNullOrWhiteSpace($env:DEEPSEEK_API_KEY)) { throw '未填写密钥，尚未启动网关。' }
    Write-Host "正在启动真实网关：http://127.0.0.1:$Port"
    Write-Host '看到 Gateway ready 后，回到 E 客户端点击“刷新模型”，选择自动或 DeepSeek 后发送问题。'
    Write-Host '保持本窗口打开；在此窗口按 Ctrl+C 退出。'
    $logPath = Join-Path $PSScriptRoot 'logs/requests.jsonl'
    & $GatewayPath --port $Port --config $ConfigPath --log $logPath
    if ($LASTEXITCODE -ne 0) { throw "网关退出，返回码：$LASTEXITCODE" }
} catch {
    Write-Host ('启动失败：' + $_.Exception.Message) -ForegroundColor Red
    exit 1
} finally {
    Remove-Item Env:DEEPSEEK_API_KEY -ErrorAction SilentlyContinue
}
