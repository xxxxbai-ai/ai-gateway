# A+B+C+D 实际模块整合

2026-11-06：最终统一构建、启动与配置持久化已移至 [release](../release/CMakeLists.txt)，使用说明见 [A交付说明](../../docs/A交付说明.md)。以下是原整合阶段记录，日期和检查组数按当时保留。新入口不再用临时 available 配置作为长期保存位置。

此入口构建正式 `gateway.exe`：A 的 HTTP 网关调用实际 B 分类、C 配置和选模、D 的 `gateway_contract::callModel`。E 的根目录 Qt 工程继续连接 `http://127.0.0.1:8080`，无需修改窗口源码。

D 的本地实现已由初次公共适配更新到 PR #9 的提交 `20561835449a31697d9de68c14cd16769fd0e9ea`，包含 512 生成预算修复、四平台配置及调用加固，沿用 D 的实现。原来 `integration/bc` 的测试替身保留在自己的测试入口内。

## 构建

需要 C++17、CMake、Ninja、libcurl 开发文件。下面是本机 Qt MinGW 工具链示例；其他电脑应替换成自己的路径。

```powershell
cmake -S integration/abcd -B build-abcd -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe -DCURL_INCLUDE_DIR=C:/Users/28904/anaconda3/Library/include -DCURL_LIBRARY=C:/Users/28904/anaconda3/Library/lib/libcurl.lib
cmake --build build-abcd --parallel 3
ctest --test-dir build-abcd --output-on-failure
```

Windows 运行时需在输出 `bin` 中提供与链接库配套的 libcurl DLL 及其依赖。开发环境中的导入库和运行时 DLL 版本应兼容；本次交付包的实际 DLL 使用 Schannel 验证 HTTPS 证书。

## 本机运行

外层工作区 `交付包/DeepSeek真实网关/` 提供完整运行文件和密钥输入脚本。先关闭之前的网关，再选择一个入口：`01-启动DeepSeek真实网关.cmd` 使用原有单模型配置；`02-启动多模型真实网关.cmd` 使用 D 的四平台配置，按提示确认各自密钥。密钥通过隐藏输入读取，只放在本次网关进程的环境中。配置和日志均不保存密钥。

启动成功后打开 E 客户端、刷新模型、使用自动模式发送“请用一句话解释 C++ 指针”。预期 `selected_model=deepseek_flash`、`is_mock=false`、`success=true`，回答来自实际服务。再手动指定该模型确认一次。

多模型入口支持 DeepSeek、千问、智谱、Kimi。它从 `config/models.providers.template.json` 读取 D 的模型参数和演示评分，按本机可用密钥生成 `config/models.available.json`，未填写密钥的模型在本次配置中禁用。模板自身不被修改；长期配置应修改模板。豆包、MiniMax 继续保持禁用。客户端刷新 `/api/models` 即可显示本次启用的模型。

多模型入口的本机检查覆盖四模型列表、部分密钥时的过滤、无密钥时拒绝启动，以及配置不含密钥值；没有使用真实密钥发出外部模型请求。用户填写各平台密钥后，应先逐个手动提问，再验证自动模式。故障切换继续使用独立模拟配置验证，不把模拟结果记为真实调用成功。

## 验证范围

`http_tests.py` 启动实际 A/B/C/D；外部模型服务由 D 的独立模拟 HTTP 服务代替，验证真实网络调用路径、自动及手动选模、最多一次备用、手动不备用、截断响应和缺失凭证错误。原 B/C、公共接口、A、D 测试也会一同运行。

2026-10-27 本机完整构建成功，CTest **20/20 组通过**。运行包在仅保留 Windows System32 的 PATH 下也可独立启动；健康检查和真实模型列表正常，自动与手动请求在缺少密钥时均经过实际 B/C/D 并返回 `missing_credential`。

这些自动测试不使用 API 密钥，不证明外部 DeepSeek 已实际回答。完整真实验收以本机输入密钥后在 E 客户端显示的回答及网关 `logs/requests.jsonl` 为准。
