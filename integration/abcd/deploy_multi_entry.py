"""Add multi-model startup files to an existing Windows gateway package."""
import argparse
from pathlib import Path
import shutil

parser = argparse.ArgumentParser()
parser.add_argument('--out', required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parents[2]
output = Path(args.out).resolve()
output.mkdir(parents=True, exist_ok=True)
(output / 'config').mkdir(exist_ok=True)
script = Path(__file__).resolve().parent / 'start-multi-model-gateway.ps1'
(output / '启动多模型真实网关.ps1').write_text(script.read_text(encoding='utf-8'), encoding='utf-8-sig')
template = output / 'config/models.providers.template.json'
if not template.exists():
    shutil.copy2(source / 'config/models.providers.example.json', template)
(output / '02-启动多模型真实网关.cmd').write_bytes((
    '@echo off\r\nchcp 65001 >nul\r\ncd /d "%~dp0"\r\n'
    'title Multi-model real gateway\r\n'
    'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0启动多模型真实网关.ps1"\r\n'
    'echo.\r\npause\r\n').encode('utf-8'))
(output / '多模型启动说明.txt').write_text('''多模型真实网关

1. 先关闭之前的网关窗口，释放 8080 端口。
2. 双击“02-启动多模型真实网关.cmd”。
3. 按窗口提示输入 DeepSeek、千问、智谱、Kimi 各自的 API 密钥。
   输入不会显示明文；直接回车表示跳过该平台，本次不在客户端列表中显示。
   已在本机环境变量配置的密钥会自动读取。
4. 看到 Gateway ready: http://127.0.0.1:8080 后，保持窗口打开。
5. 回到现有客户端，点击“刷新模型”。填写了四个平台密钥，应出现四个真实模型和自动选择。
6. 先手动逐个选择模型，分别发送“请只回答：测试通过。”，验证账号权限与真实回答。
7. 再选择自动模式，发送编程、数学、翻译或写作问题，观察实际选择的模型和理由。

四个平台分别需要各自的密钥：
DeepSeek：DEEPSEEK_API_KEY
千问：DASHSCOPE_API_KEY（北京兼容接口）
智谱：ZHIPU_API_KEY
Kimi：MOONSHOT_API_KEY

配置来自 D 提交的 models.providers.example.json，保留模型名称、生成上限和演示评分。
豆包、MiniMax 仍为禁用模板。评分是人工演示配置，不代表真实模型质量测评。
填写密钥表示客户端可以尝试调用；每个平台是否成功仍需实际提问验证。

密钥只放在本次进程环境，配置和日志均不保存密钥，也不上传 GitHub。
日志位于 logs/multi-requests.jsonl。
每次启动会根据模板和本次可用密钥生成 config/models.available.json。
需要长期调整评分或启停项时，修改 config/models.providers.template.json。
旧“01”入口继续使用原来的 DeepSeek 单模型配置。
''', encoding='utf-8')
print('Multi-model startup entry prepared: ' + str(output))
