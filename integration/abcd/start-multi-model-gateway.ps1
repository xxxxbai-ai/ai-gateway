param(
    [string]$GatewayPath = (Join-Path $PSScriptRoot 'gateway.exe'),
    [string]$TemplatePath = (Join-Path $PSScriptRoot 'config/models.json'),
    [string]$AvailableConfigPath = (Join-Path $PSScriptRoot 'config/models.available.json'),
    [ValidateRange(0, 65535)][int]$Port = 8080,
    [switch]$NoPrompt,
    [switch]$PrepareOnly
)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$originalEnvironment = @{}

function Set-SessionCredential([string]$Variable, [string]$Value) {
    if (-not $originalEnvironment.ContainsKey($Variable)) {
        $originalEnvironment[$Variable] = [Environment]::GetEnvironmentVariable($Variable, 'Process')
    }
    [Environment]::SetEnvironmentVariable($Variable, $Value, 'Process')
}

function Resolve-ModelCredential([string]$Variable, [string]$DisplayName) {
    $credentialValue = [Environment]::GetEnvironmentVariable($Variable, 'Process')
    foreach ($credentialScope in @('User', 'Machine')) {
        if ([string]::IsNullOrWhiteSpace($credentialValue)) {
            $credentialValue = [Environment]::GetEnvironmentVariable($Variable, $credentialScope)
        }
    }
    if ([string]::IsNullOrWhiteSpace($credentialValue) -and -not $NoPrompt) {
        $secureCredential = Read-Host "$DisplayName API 密钥（直接回车跳过）" -AsSecureString
        try {
            if ($secureCredential.Length -gt 0) {
                $credentialPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secureCredential)
                try { $credentialValue = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($credentialPointer).Trim() }
                finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($credentialPointer) }
            }
        } finally { $secureCredential.Dispose() }
    }
    if ([string]::IsNullOrWhiteSpace($credentialValue)) { return $false }
    if ($credentialValue.Contains("`r") -or $credentialValue.Contains("`n")) {
        throw "$DisplayName 的密钥包含换行，请重新输入。"
    }
    Set-SessionCredential $Variable $credentialValue
    $credentialValue = $null
    return $true
}

try {
    if (-not (Test-Path -LiteralPath $GatewayPath -PathType Leaf)) { throw '找不到网关程序，请使用完整的运行文件夹。' }
    if (-not (Test-Path -LiteralPath $TemplatePath -PathType Leaf)) { throw '找不到多模型配置模板。' }
    & $GatewayPath --check-config --config $TemplatePath
    if ($LASTEXITCODE -ne 0) { throw '模型配置未通过网关校验，请检查 config/models.json。' }
    if (-not $PrepareOnly -and $Port -gt 0) {
        $listeners = [System.Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()
        if ($listeners | Where-Object { $_.Port -eq $Port }) {
            throw "端口 $Port 已被占用。请先关闭之前的网关窗口，再启动多模型网关。"
        }
    }
    $configuration = Get-Content -LiteralPath $TemplatePath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($configuration.contract_version -ne '1.0' -or $null -eq $configuration.models) {
        throw '多模型配置格式不正确。'
    }
    Write-Host '多模型真实网关：请依次确认各平台密钥。输入不会显示明文，直接回车可跳过该模型。'
    $enabledNames = @()
    foreach ($model in $configuration.models) {
        if (-not $model.enabled) { continue }
        if ($model.is_mock) { $enabledNames += [string]$model.name; continue }
        $reference = [string]$model.credential_ref
        if ($reference -notmatch '^env:([A-Za-z_][A-Za-z0-9_]*)$') { throw '模型凭证引用格式不正确。' }
        $variableName = $Matches[1]
        if (Resolve-ModelCredential $variableName ([string]$model.name)) {
            $enabledNames += [string]$model.name
            Write-Host ("已配置：" + $model.name)
        } else {
            $model.enabled = $false
            Write-Host ("未配置密钥，本次跳过：" + $model.name)
        }
    }
    if ($enabledNames.Count -eq 0) { throw '没有配置任何可用模型，尚未启动网关。' }
    $configDirectory = Split-Path -Parent $AvailableConfigPath
    [IO.Directory]::CreateDirectory($configDirectory) | Out-Null
    $json = $configuration | ConvertTo-Json -Depth 20
    [IO.File]::WriteAllText($AvailableConfigPath, $json, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host ('本次启用：' + ($enabledNames -join '、'))
    if ($PrepareOnly) { exit 0 }
    Write-Host '看到 Gateway ready 后，回到客户端点击“刷新模型”，即可自动选择或手动指定模型。'
    Write-Host 'config/models.json 保存长期设置；本次缺少密钥不会改变其中的启停和评分。'
    Write-Host '密钥仅用于本次网关进程；配置和日志不保存密钥。保持此窗口打开，按 Ctrl+C 退出。'
    $logPath = Join-Path $PSScriptRoot 'logs/multi-requests.jsonl'
    # Runtime credential filtering belongs to A. C always loads/saves the durable config.
    & $GatewayPath --port $Port --config $TemplatePath --require-credentials --log $logPath
    if ($LASTEXITCODE -ne 0) { throw "网关退出，返回码：$LASTEXITCODE" }
} catch {
    Write-Host ('启动失败：' + $_.Exception.Message) -ForegroundColor Red
    exit 1
} finally {
    foreach ($variableName in $originalEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($variableName, $originalEnvironment[$variableName], 'Process')
    }
}
