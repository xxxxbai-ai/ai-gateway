# AI Gateway 桌面客户端 1.0

独立重写的 C++17 / Qt 6 Widgets 客户端。暖米灰与鼠尾草绿主题，连接现有 A 网关的公共 HTTP 接口。旧版 E 窗口与网关模块没有被覆盖。

## 运行

完整运行包中双击 `AIGatewayClient.exe`，无需安装 Qt Creator。

1. 先按原流程启动网关，填写密钥，看到 `Gateway ready`。
2. 打开客户端。默认连接 `http://127.0.0.1:8080`，启动时获取可用模型。
3. 自动选择或手动选择模型，输入问题。Enter 发送，Shift+Enter 换行。
4. 顶部刷新按钮可重新获取模型；左下角设置可以修改网关地址。本机支持 HTTP，远程地址使用 HTTPS。

客户端不收集、保存或向聊天中发送模型平台密钥。实际模型密钥继续由网关管理。

## 已实现

- 正常真实回答不再显示【真实】；模拟回答仍明确标注。
- Markdown 段落、标题、粗体、列表、引用、表格、代码块；完整回答和代码可复制。
- 实际模型名称按响应 selected_model 映射，不使用发送后随意变化的下拉框来猜测；备用模型显示最终作答者。
- 手动模式不重复显示选模理由；自动模式提供折叠的白话说明；原始判断、请求编号等留在详情。
- 会话在本机持久保存，支持搜索、新建、切换、右键重命名/删除、导出 Markdown；使用原子写入，异常历史保留备份。
- 发送与停止等待、130 秒客户端超时、模型列表超时和刷新、空列表清理、HTTP/JSON 错误处理。
- 每次请求绑定原会话；在等待时新建/切换不会串答。当前采用一次处理一个问题的方式，其他会话可先编辑草稿。
- 超时和停止会中止本地网络接收，并忽略旧回调；不承诺上游服务一定停止计费。
- 返回内容不执行原始 HTML，不加载回答中的本地/远程图片。外部链接仅在用户点击正常 HTTP/HTTPS 链接时打开。

当前网关按单条 message 回答。客户端保存历史供用户查看，不会把这些历史当作多轮上下文发送。双回答投票、评分学习、流式输出与托管账号体系没有在界面中伪装为已完成。

## 本机数据

Windows 默认位置由 `QStandardPaths::AppLocalDataLocation` 决定，通常为：

`%LOCALAPPDATA%\AI Gateway Team\AIGatewayClient\`

`history.json` 是本机明文历史；`preferences.ini` 保存网关地址和窗口设置，不含模型密钥。具体目录可在客户端设置中查看和打开。要分享运行包，只分享程序文件夹，不附带本机历史。

命令行可指定隔离数据目录与网关：

```powershell
.\AIGatewayClient.exe --data-dir "D:\我的聊天记录" --gateway "http://127.0.0.1:8080"
```

## 构建与测试

Qt 6.5 以上，Core / Gui / Widgets / Network；测试还需 Qt Test。本机使用 Qt 6.11.2 与配套 MinGW 13.1。

```powershell
cmake -S client -B build-client -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build-client --parallel 3
ctest --test-dir build-client --output-on-failure
```

也可以直接用 Qt Creator 打开本目录 `CMakeLists.txt`。输出在构建目录 `bin/AIGatewayClient.exe`。

测试使用本机独立 HTTP 服务，覆盖实际 Qt 网络与窗口逻辑，不调用收费模型。包括重启恢复、损坏历史备份、Markdown/表格、错误 JSON、超时/取消/迟到回调、最终模型映射、跨会话归属、空模型列表恢复和键盘行为。测试窗口设置为不显示到桌面；Windows 使用原生字体绘制，其余平台使用 offscreen。

## 源码位置

- `conversation.*`：历史数据、原子保存和导出。
- `gateway_client.*`：模型列表、聊天请求、超时和取消。
- `widgets.*`：主题、输入框、Markdown、聊天卡片和历史列表。
- `mainwindow.*`：窗口布局、会话管理和网关交互。
- `tests/client_tests.cpp`：独立本机协议和 Qt 交互验收。

本客户端只使用 `/api/models` 和 `/api/chat`；不向普通用户提供全局模型评分修改入口。
