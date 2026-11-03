# D 模块接口与验收记录

- 模块：模型接入（D）
- 分支：`codex/d-verify-domestic-models-20261027`
- 相关提交：`e23b04c`、`0ce82f9`、`fb105d7`、`4367911`、`94519a4`
- 对照接口：`shared/contracts.h`、`shared/module_api.h`（公共接口 1.0）
- 记录日期：2026-11-03

## 1. 交付范围

D 负责把 C 选出的一个 `ModelConfig` 变成一次上游调用，返回完整回答或结构化失败；不选模、不跨模型重试、不决定是否备用。

代码位置：

| 内容 | 位置 |
| --- | --- |
| 公共入口 | `ai-gateway/src/providers/call_model.cpp` |
| 适配器与传输 | `ai-gateway/src/providers/provider_types.h`、`provider_adapter.*`、`compatible_chat_adapter.*` |
| SSE 解析 | `ai-gateway/src/providers/sse_decoder.*` |
| 凭证 | `ai-gateway/src/providers/credential_resolver.*` |
| 进程内模拟 | `ai-gateway/src/providers/mock_provider.*` |
| 独立模拟上游 | `ai-gateway/tools/mock_provider/` |
| 适配器命令行 | `ai-gateway/tools/provider_cli/` |
| 公共入口命令行 | `ai-gateway/tools/public_call_cli/` |
| 测试 | `ai-gateway/tests/providers/provider_tests.cpp`、`public_call_tests.cpp` |
| 配置 | `config/models.example.json`、`config/models.deepseek.example.json`、`config/models.providers.example.json` |

## 2. 构建与运行

```powershell
# 仓库根目录
cmake -S ai-gateway -B ai-gateway/build-d -G Ninja
cmake --build ai-gateway/build-d

# 终端 1：启动模拟上游
.\ai-gateway\build-d\mock_provider.exe --port 18080 --delay-ms 100

# 终端 2：适配器级调用
.\ai-gateway\build-d\provider_cli.exe --stream --message "解释什么是 API 网关"

# 公共入口调用（模拟模型）
.\ai-gateway\build-d\public_call_cli.exe --config config\models.example.json --model mock_code --message "解释什么是 API 网关"

# 公共入口调用（真实模型，先设置环境变量）
$env:DEEPSEEK_API_KEY = "..."
.\ai-gateway\build-d\public_call_cli.exe --config config\models.providers.example.json --model deepseek_flash --max-tokens 64 --message "只回复 OK"
```

`public_call_cli` 现在支持 `--timeout-ms`、`--connect-timeout-ms`、`--first-timeout-ms`、`--max-tokens`；默认配置找不到时会按可执行文件位置回退到仓库 `config/`。

> 本轮为支持 `usage` 和按模型 `max_tokens`，在 `shared/contracts.h` 增加了可选字段，并让 C 的 `contract_adapter.cpp` 解析/保存 `max_tokens`。这是跨模块改动，合并前需要 A/C 确认。

## 3. 自动化测试

| 测试组 | 覆盖点 | 结果 |
| --- | --- | --- |
| `public_call_model` | 公共 `callModel` 成功、单次调用、错误映射、上下文复用、取消、截止、usage、默认/按模型 max_tokens、慢完成、异常 usage | PASS |
| `provider_sse` | 任意切分、逐字节、CRLF、多行 `data`、不完整事件 | PASS |
| `provider_calls` | 三模型、别名、历史、usage、缺凭证、空消息、非法 UTF-8、异常 usage、非流式首字节、响应上限 | PASS |
| `provider_stream` | normal/fragmented/packed/no_usage，五段拼接一致 | PASS |
| `provider_faults` | 429/503/401、断流、缺 `[DONE]`、坏 JSON、首超时、共享 deadline、上游错误码 | PASS |
| `provider_cancel` | 预取消、回调取消、外部取消、回调异常、连接释放 | PASS |
| `provider_concurrent` | 8 线程并发调用、请求编号不串、计数正确 | PASS |
| `provider_gateway_route` | B/C/D 串联、三模型分流、历史、手动、未知模型 | PASS |
| `provider_gateway_fallback` | 429/503 备用、手动不换、最多两次、共享 deadline、已输出不换 | PASS |

运行方式：

```powershell
.\ai-gateway\build-d\provider_tests.exe sse
.\ai-gateway\build-d\provider_tests.exe calls
.\ai-gateway\build-d\provider_tests.exe stream
.\ai-gateway\build-d\provider_tests.exe faults
.\ai-gateway\build-d\provider_tests.exe cancel
.\ai-gateway\build-d\provider_tests.exe concurrent
.\ai-gateway\build-d\provider_tests.exe gateway_route
.\ai-gateway\build-d\provider_tests.exe gateway_fallback
.\ai-gateway\build-d\public_call_tests.exe
```

## 4. 模拟调用记录

2026-11-03 在本机复跑（模拟服务，`--delay-ms 100`）：

| 场景 | 实际输出 |
| --- | --- |
| `provider_cli --stream` 正常 | `success=true`、HTTP 200、`elapsed_ms=545`、usage 12/20/32 |
| `provider_cli --base-url .../reject503/v1 --stream` | `upstream_rejected`、HTTP 503、`explicit_rejection=true` |
| `provider_cli --base-url .../disconnect/v1 --stream` | `incomplete_response`、HTTP 200、半截回答、`success=false` |
| `provider_cli --base-url .../delay/v1 --first-timeout-ms 80` | `first_response_timeout`、HTTP 0 |
| `provider_cli --base-url .../delay/v1 --cancel-after-ms 50` | `cancelled`、HTTP 0 |
| `public_call_cli --config config\models.example.json --model mock_code` | `success=true`、`model_id=mock_code`、`provider_model=demo-code`、HTTP 200 |

这些是模拟结果，只证明调用链、超时、取消和错误映射按规则工作，不代表真实模型质量。

## 5. 真实模型调用记录

### 5.1 2026-10-27 首次验证

| 项目 | 内容 |
| --- | --- |
| 日期 | 2026-10-27 |
| 公共编号 / 上游模型 | `deepseek_flash` / `deepseek-flash` |
| 地址 / 协议 | `https://api.deepseek.com` / `chat_completions` |
| 凭证 | `DEEPSEEK_API_KEY`（仅进程环境，未入库） |
| 结果 | HTTP 200、`success=true`、回答非空（提交 `0ce82f9` 提交说明） |
| 请求编号 | 未保存（当时未落盘） |
| 耗时 / 回答长度 | 未保存（当时未落盘） |
| 后续验证 | `94519a4` 启用已验通的 Qwen/Kimi/GLM；豆包、MiniMax 保持禁用模板 |

### 5.2 2026-11-03 最小复验

| 项目 | 内容 |
| --- | --- |
| 公共编号 / 上游模型 | `deepseek_flash` / `deepseek-flash` |
| 地址 / 协议 | `https://api.deepseek.com` / `chat_completions` |
| 凭证 | `DEEPSEEK_API_KEY`（仅进程环境，未入库） |
| 请求 | `只回复 OK`，`max_tokens=64` |
| 结果 | HTTP 200、`success=true`、回答 `OK` |
| 请求编号 | `d-1793692498662820` |
| 耗时 | 584 ms |
| 用量 | 输入 33、输出 39、总计 72，`simulated=false` |
| 回答长度 / 摘要 | 2 字符 / SHA-256 前 8 位 `565339bc` |

**说明：** 按小组约定，密钥和回答原文不写入仓库、日志或报告。5.2 的回答只有 `OK`，用长度和哈希留证；5.1 的“未保存”字段是事实记录，不是事后补造。两次都只验证通路和字段，不代表回答质量。

## 6. 本轮优化内容

| 优化 | 行为 |
| --- | --- |
| 非流式首响应 | `Transfer` 增加 `received_first_byte`：流式按首个非空 delta、非流式按首个响应体字节计算 30 秒首响应；总预算仍为 120 秒 |
| 异常 `usage` | `readUsage` 改为尽力而为：字段缺失或格式错误只丢弃用量，不再把完整回答判成 `invalid_response` |
| 响应大小上限 | `RequestContext.max_response_bytes` 默认 8 MiB；非流式 body 和流式回答超限返回 `invalid_response` |
| 上游错误码 | 非 2xx 只保留最多 4 KiB body，提取 `error.code/type`（字符集受限、最长 40 字符）附加到错误信息；不拼接供应商原文 |
| 并发测试 | 新增 `provider_concurrent`：8 线程并发调用，验证请求编号、计数和线程安全 |
| CLI 可用性 | `public_call_cli` 支持三个超时参数和 `--max-tokens`；默认配置按可执行文件位置回退，减少“从错误目录运行”的失败 |
| 公共用量 | `CallResult` 增加可选 `Usage`；D 映射内部 `AttemptResult.usage`，CLI 输出 `usage` |
| 按模型 `max_tokens` | `ModelConfig` 增加可选 `max_tokens`；D 优先使用，C 的配置读写同步支持 |

## 7. 仍待处理

1. **真实调用证据**：5.1 的请求编号、耗时、回答长度未落盘；5.2 已补全最小复验的请求编号、耗时、用量和回答哈希。后续真实长回答也应保留同样的脱敏字段。
2. **配置评分**：`config/models.providers.example.json` 已从全 80 改为差异化演示评分，自动路由会按任务选到不同模型；这些分数仍是人工预设，需用固定问题集校准。
3. **CI**：`.github/workflows/d-provider-ci.yml` 已加（Windows + MSYS2，构建并跑 9 组测试），尚未在 GitHub 实际跑过；首次推送后需确认结果。
4. **合并确认**：`shared/contracts.h` 和 `modules/router/contract_adapter.cpp` 的改动需要 A/C 确认；如果公共 1.0 暂不扩展，应回退为 D 侧日志方案。

## 8. 结论

D 的公共入口、模拟上游、真实模型配置和 9 组测试可以在本机复现；真实模型已有 2026-11-03 的可核对记录（请求编号、耗时、用量、回答哈希）。整链验收仍需要 A 把 `callModel` 接入 HTTP `POST /api/chat`，并由 E 展示 `is_mock`、`fallback_used` 和错误码。公共 `usage`/`max_tokens` 字段属于跨模块提案，合并前需 A/C 确认。
