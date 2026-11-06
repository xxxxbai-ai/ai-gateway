"""Package only the client, without credentials, personal history or gateway changes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--qt-bin", type=Path, required=True)
parser.add_argument("--compiler-bin", type=Path, required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent.parent
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
executable = args.build.resolve() / "bin" / "AIGatewayClient.exe"
if not executable.is_file():
    raise SystemExit("Build AIGatewayClient first")
shutil.copy2(executable, output / executable.name)
environment = dict(os.environ)
environment["PATH"] = str(args.qt_bin) + os.pathsep + str(args.compiler_bin) + os.pathsep + environment.get("PATH", "")
result = subprocess.run([str(args.qt_bin / "windeployqt.exe"), "--release", "--compiler-runtime",
                         "--no-translations", "--no-opengl-sw", "--dir", str(output),
                         str(output / executable.name)], env=environment, check=True,
                        capture_output=True, text=True, errors="replace", timeout=120,
                        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
# Offscreen enables verification of the real packaged application without a desktop window.
offscreen = args.qt_bin.parent / "plugins" / "platforms" / "qoffscreen.dll"
if offscreen.is_file():
    shutil.copy2(offscreen, output / "platforms" / offscreen.name)
translation = args.qt_bin.parent / "translations" / "qtbase_zh_CN.qm"
if translation.is_file():
    (output / "translations").mkdir(exist_ok=True)
    shutil.copy2(translation, output / "translations" / translation.name)
qt_licenses = args.qt_bin.parent.parent.parent / "Licenses"
(output / "third-party").mkdir(exist_ok=True)
if qt_licenses.is_dir():
    (output / "third-party").mkdir(exist_ok=True)
    for name in ("LICENSE", "Copyright.txt"):
        if (qt_licenses / name).is_file():
            shutil.copy2(qt_licenses / name, output / "third-party" / ("Qt-" + name))
(output / "third-party" / "Qt-runtime.txt").write_text(
    "Qt 6.11.2 Core / GUI / Widgets / Network, dynamically linked.\n"
    "Homepage: https://www.qt.io/\nSource: https://code.qt.io/cgit/qt/qtbase.git/\n"
    "Runtime files are copied from the locally installed Qt SDK.\n", encoding="utf-8")
shutil.copy2(source / "README.md", output / "源码与功能说明.md")
test_log = args.build.resolve() / "client-tests.txt"
if test_log.is_file():
    shutil.copy2(test_log, output / "客户端测试结果.txt")
instructions = """AI Gateway 客户端 1.0

双击 AIGatewayClient.exe 打开，不需要安装 Qt Creator。

第一次使用：
1. 先启动原来的网关，在网关窗口填写模型密钥。
   当前课程包：相邻的“DeepSeek真实网关”文件夹中，打开“02-启动多模型真实网关.cmd”。
2. 出现 Gateway ready 后，打开客户端。
3. 自动选择或手动选择模型，输入问题。

Enter 发送；Shift+Enter 换行。等待时可以停止，或先切换会话写下新问题。
左侧历史自动保存在这台电脑，重启仍可查看；右键可重命名或删除。
顶部按钮可导出当前对话；左下角设置可以修改网关地址、查看历史目录。

当前网关按每条问题独立回答。历史供你查看，不代表模型会记住前文。
“停止等待”会停止客户端接收；上游是否停止由网关和模型服务决定。

程序里的“模拟演示”只用于标明模拟回答，正常真实回答没有【真实】标签。
界面里的实际模型名称以网关响应为准；请求编号和分类可在每条回答“详情”中查看。

请保留整个程序文件夹，不要只移动 exe。分享运行包不需要附带你电脑里的历史。
模型平台密钥由网关管理，这个客户端运行包不包含密钥。
"""
(output / "先看这里.txt").write_text(instructions, encoding="utf-8-sig")
source_files = sorted(p for p in source.rglob("*") if p.is_file() and not any(part.startswith("build") or part=="__pycache__" for part in p.relative_to(source).parts))
manifest = {"client_version": "1.0.0", "gateway_contract": "GET /api/models; POST /api/chat",
            "compiler": "Qt MinGW 13.1 x64", "qt": "6.11.2", "contains_credentials": False,
            "sha256": {str(p.relative_to(source)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files},
            "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}
(output / "构建来源.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
source_zip = output.parent / "AI Gateway客户端V1.0_源码.zip"
with zipfile.ZipFile(source_zip, "w", zipfile.ZIP_DEFLATED) as archive:
    for path in source_files:
        archive.write(path, "client/" + path.relative_to(source).as_posix())
package_zip = output.parent / "AI Gateway客户端V1.0_运行包.zip"
with zipfile.ZipFile(package_zip, "w", zipfile.ZIP_DEFLATED) as archive:
    for path in output.rglob("*"):
        if path.is_file():
            archive.write(path, output.name + "/" + path.relative_to(output).as_posix())
print(json.dumps({"package": str(output), "runtime_zip": str(package_zip),
                  "source_zip": str(source_zip), "runtime_bytes": package_zip.stat().st_size,
                  "executable_sha256": manifest["executable_sha256"]}, ensure_ascii=False))
