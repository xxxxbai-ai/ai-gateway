# 本轮只提交 A 网关，各自修改各自的代码

日期：2026-10-27。

本轮协作入口是 A 网关交接版。之前的跨成员整合演示涉及 B/C/D/E 的修改，已经停止作为合入方案；本地演示包仅供参考。
本次新提交只增加 `gateway/`、A 维护的两个公共头文件和本说明，其他成员的实现文件没有改动。
仓库主分支本来已有的成员代码会出现在分支中，但不属于这次的修改；查看合并申请的“Files changed”可核对范围。

**每位成员保留自己的实现，补齐交接处，再交给 A 联调。不要求复制之前的整合窗口或模块实现。**

## A 已交付什么

- HTTP 接收与输入检查；统一成功/失败回复。
- 调用 B 分类、C 读配置与选模、D 取得回答的流程。
- 自动最多尝试首选和一个不同备用；手动失败不换模型。
- 请求编号、总时限、错误映射与运行日志。
- 模型列表与设置入口；保存动作交给 C。
- A 的独立测试和构建说明，见 `gateway/README.md`。

## B：把已有分类函数交给 A 调用

当前主分支的 `classifier.cpp` 含 `QueryClassifier::classify` 和命令行 `main`，公共入口尚未在这条主分支提供。

请 B 在自己的代码中：

1. 提供 `gateway_contract::analyzeQuestion(const std::string&)`，具体返回类型见 `shared/module_api.h`。
2. 内部继续使用自己的分类规则，返回 `task/reason/uncertain`。
3. 类型统一为 `programming/math/translation/writing/general`；拿不准时返回 `general` 并说明原因。
4. 将可复用函数与命令行 `main` 分开编译，避免和网关的 `main` 冲突。
5. 自行检查“解释 C++ 指针”“计算256的平方根”“把你好翻译成英文”“写一封请假信”“你好”，提供实际结果。

之前本地整合版补过“请假信”规则，但本轮没有替 B 修改规则；是否加入、如何处理，由 B 在自己的版本里完成。

## C：保留已有选模算法，补配置和公共入口

当前 `modules/router/model_router.cpp/.h` 已有 `routing::selectModel(task, models)`，返回单个结果。
它使用的类型与公共头文件不同，不能直接把 A 的参数交过去。

请 C 在自己的代码中提供公共入口：

- `loadModels`：读取当前模型配置，失败时返回错误；配置同时提供 D 所需的服务地址、实际模型名和凭证引用。
- `saveModels`：检查并可靠保存设置；失败保留旧文件，重启后仍能读取保存内容。
- `selectModel`：接收分类结果、用户模式和模型列表，返回首选及至多一个不同备用。

自动模式过滤禁用项及无效评分，沿用已有最高分及同分固定排序。手动模式校验模型存在、启用，只返回该模型，不提供备用。
能力分沿用 0–100，五类名称与公共头文件一致，属于项目预设值。

请提供配置文件、实现文件清单、内部依赖与构建方法，验证评分变化、禁用、未知模型、无可用模型、保存重启。
本轮没有加入之前代写的 `public_router.cpp`，由 C 自己决定包装文件的位置和实现。

## D：把已有网络调用转换成公共返回格式

当前 `ai-gateway/src/providers/compatible_chat_adapter.cpp` 已有调用实现，类型在 D 自己的 `gateway` 名字空间。
A 使用 `gateway_contract` 的类型，两者需要转换；已有网络调用代码可以继续使用。

请 D 在自己的代码中提供公共 `callModel(model, request, context)`：

- 将公共的 `message` 转成内部的 `question`，将 `is_mock` 对应到内部 `simulated` 等字段。
- 使用配置中的 `provider_model/base_url/credential_ref/protocol`，凭证只用环境变量引用。
- 沿用 A 传入的请求编号、截止时间和取消状态，不在备用调用时重置总时限。
- 只调用指定模型一次，返回实际模型编号、完整回答、耗时、错误、HTTP 状态及是否明确拒绝。
- 失败不伪装成功；不在 D 内部自行跨模型重试，备用由 A 决定。

请 D 给出模拟服务启动方式和可复现的 429/503 失败场景，供 A 检查切换。模拟文字明确标注即可，不要求照搬本地整合版的回显内容。
真实账号目前暂缺；先完成模拟对接，真实模型成功记录待账号可用后再补。
本轮没有加入之前代写的 `public_provider.cpp`，也没有修改 D 的模拟服务或测试文件。

## E：保留自己的窗口，按消息字段调整收发

检查的是主分支现有 `mainwindow.cpp`，具体待办如下：

1. `onNetworkReply` 当前读 `jsonObj["model"]`，请改为读取约定的 `selected_model`，否则会显示错误的作答模型。
2. 当前下拉框写死三个模型。请请求 `/api/models` 动态生成，并保留“自动”选项。
3. 当前只在 `QNetworkReply::NoError` 时解析 JSON。HTTP 400/500 等也可能包含网关的 `error`，请先读取状态与响应内容，再区分业务错误和真正的断线。
4. 显示 `task/reason/request_id/fallback_used/is_mock`，让用户看见题型、实际作答模型和是否切换。
5. 发送时禁用重复发送，结束或失败后恢复；空问题有提示，并设置等待超时。
6. 保留现有自动/手动请求格式：自动发送 `model:null`，手动发送模型编号。发送字段仍是 `message`。

不要求改成之前重写的界面。模型设置窗口、自动启动网关等可在基础对接通过后由 E 自己决定实现。
本版不接受 `demo_failure:true`；故障场景使用 D 提供的测试服务或配置，不把之前的演示开关当成公共协议。

## 窗口与 A 的最小消息样例

自动请求 `POST /api/chat`：

```json
{"mode":"auto","model":null,"message":"解释 C++ 指针"}
```

手动请求：

```json
{"mode":"manual","model":"C配置中的模型编号","message":"解释 C++ 指针"}
```

成功回复中 `selected_model` 是最终成功模型：

```json
{"request_id":"示例编号","success":true,"task":"programming","uncertain":false,"selected_model":"示例模型编号","is_mock":true,"reason":"B与C提供的实际理由","answer":"D取得的回答","fallback_used":false}
```

失败回复：

```json
{"request_id":"示例编号","success":false,"error_code":"invalid_request","error":"问题不能为空","fallback_used":false}
```

字段类型及函数签名以 `shared/contracts.h`、`shared/module_api.h` 为准。公共接口版本仍为 1.0；本轮没有重新更改交接名称。
设置消息：`GET /api/config` 返回 `success/models`；`POST /api/config` 接收 `{"models":[{"id":"编号","enabled":true,"scores":{"programming":80,"math":80,"translation":80,"writing":80,"general":80}}]}`，只更新列出的模型。

## 各自完成后交给 A 什么

每人提供自己的提交/分支、实现文件及依赖清单、构建运行方法、实际输入输出和未完成项。
待 B/C/D 的五个公共函数实现齐全，再由 A 连接为正式网关；E 用自己的窗口验证收发。
A 的测试替身只证明 A 按约定传递和处理结果，不代表 B 分类、C 配置保存、D 网络或 E 窗口已经通过联调。
