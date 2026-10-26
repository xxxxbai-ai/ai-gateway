"""Create a fresh Windows mock-demo folder and ZIP from a deployed build."""
import argparse
from pathlib import Path
import shutil
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--bin', required=True, type=Path)
parser.add_argument('--out', required=True, type=Path)
parser.add_argument('--qt-licenses', type=Path)
options = parser.parse_args()
source = Path(__file__).resolve().parents[1]
binary = options.bin.resolve()
output = options.out.resolve()
if output.exists():
    raise SystemExit('Output already exists; choose a fresh folder to preserve saved settings.')
for required in ['MyClient.exe', 'gateway.exe', 'Qt6Core.dll', 'Qt6Widgets.dll', 'Qt6Network.dll',
                 'libcurl.dll', 'platforms/qwindows.dll']:
    if not (binary / required).is_file():
        raise SystemExit('Missing runtime file: ' + required)
output.mkdir(parents=True)
for name in ['MyClient.exe', 'gateway.exe']:
    shutil.copy2(binary / name, output / name)
for library in binary.glob('*.dll'):
    if library.name != 'Qt6Test.dll':
        shutil.copy2(library, output / library.name)
for name in ['platforms', 'styles', 'imageformats', 'iconengines', 'networkinformation', 'tls', 'generic']:
    if (binary / name).is_dir():
        shutil.copytree(binary / name, output / name)
(output / 'config').mkdir()
shutil.copy2(source / 'config/models.example.json', output / 'config/models.json')
shutil.copy2(source / 'config/models.example.json', output / 'config/models.default.json')
(output / '使用说明.txt').write_text('''AI 网关 · 模拟演示版

一、怎么打开
完整解压整个压缩包，双击 MyClient.exe。等待右上角显示“已连接”。
不需要安装 Qt、不需要模型账号，也不用另外打开终端。
不要只发一个 exe，请把整个压缩包发给组员。

二、按这个顺序演示
1. 保持顶部“自动选择模型”，发送“解释 C++ 指针”。观察编程类型和模拟编程模型。
2. 发送“求解方程 x+2=5”。观察数学类型和模拟数学模型。
3. 发送“把你好翻译成英文”或“写一封请假信”。观察翻译、写作类型和模拟通用模型。
4. 在顶部手动选择“模拟数学模型”，再发送编程问题。观察这次按手动选择回答。
5. 切回自动，勾选“让首选模型失败”，发送编程问题。观察备用模型接手。
6. 保持故障开关，手动选择一个模型。此次直接提示失败，不会偷偷换模型。
7. 关闭故障开关，打开“模型设置”，把模拟数学模型的编程分改成 100 后保存。
   切回自动，发送编程问题，观察选模发生变化。退出重开，设置仍保留。

三、需要说清的范围
本版已完成完整模拟流程，回答会重复问题来证明收发接通。
“模拟回答”不是 AI 的真实答案，不会真的解题或翻译。
每次只处理当前问题，不记住之前的聊天。能力分是项目预设值。
真实模型账号暂缺，真实模型调用仍待验证。
当前已在开发电脑验证，换到其他电脑后仍需试运行一次。

四、遇到问题
连接中断：点“重新连接”。
配置损坏或要恢复默认：先关闭窗口，把 config/models.default.json 复制为 models.json 后重开。
恢复默认会覆盖自己保存的分数。需要保留时先复制一份 models.json。
运行日志在 logs/requests.jsonl，只记录流程和耗时，不记录问题原文或密钥。
退出窗口后，程序自动关闭自己启动的网关。
''', encoding='utf-8-sig')
licenses = output / '第三方许可'
licenses.mkdir()
shutil.copy2(source / 'ai-gateway/third_party/cpp-httplib/LICENSE', licenses / 'cpp-httplib.txt')
shutil.copy2(source / 'ai-gateway/third_party/nlohmann/LICENSE.MIT', licenses / 'nlohmann-json.txt')
if options.qt_licenses and options.qt_licenses.is_dir():
    shutil.copytree(options.qt_licenses, licenses / 'Qt')
(licenses / '来源.txt').write_text('''本演示复用 Qt、libcurl、cpp-httplib、nlohmann/json 及各自运行依赖。
Qt: https://www.qt.io/licensing/ ，https://code.qt.io/cgit/qt/qtbase.git/
libcurl: https://curl.se/docs/copyright.html ，https://curl.se/download.html
cpp-httplib 与 nlohmann/json 的许可已随附，源码随项目仓库提供。
其他 DLL 由本机 Qt/MinGW 和 Anaconda 运行库整理而来，保留原始署名。
项目源码：https://github.com/xxxxbai-ai/ai-gateway/tree/codex/integrated-demo-20261026
''', encoding='utf-8-sig')
archive = output.with_suffix('.zip')
if archive.exists():
    raise SystemExit('ZIP already exists; choose a fresh output name.')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as package:
    for path in sorted(output.rglob('*')):
        if path.is_file():
            package.write(path, path.relative_to(output.parent))
print('Demo folder: ' + str(output))
print('ZIP: ' + str(archive))
print('ZIP size: %.1f MB' % (archive.stat().st_size / 1024 ** 2))
