"""Create a reproducible, credential-free course delivery from a committed integration."""
import argparse, hashlib, json, os, re, shutil, subprocess, sys, zipfile
from pathlib import Path

p=argparse.ArgumentParser()
for name in ('build','out','curl-bin','compiler-bin','qt-bin'):p.add_argument('--'+name,required=True,type=Path)
p.add_argument('--runtime-only',action='store_true',help='Prepare dependencies before running CTest')
a=p.parse_args();source=Path(__file__).resolve().parents[2];binary=a.build.resolve()/'bin'

def deploy_curl(destination):
    destination.mkdir(parents=True,exist_ok=True)
    search=[a.curl_bin,a.compiler_bin];pending=[a.curl_bin/'libcurl.dll'];copied=set()
    for name in ('libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll'):
        shutil.copy2(a.compiler_bin/name,destination/name)
    while pending:
        path=pending.pop()
        if path.name.lower() in copied:continue
        copied.add(path.name.lower());shutil.copy2(path,destination/path.name)
        imports=subprocess.check_output([str(a.compiler_bin/'objdump.exe'),'-p',str(path)])
        for name in re.findall(r'DLL Name:\s*(\S+)',imports.decode(errors='replace')):
            for directory in search:
                candidate=directory/name
                if candidate.is_file():pending.append(candidate);break
    return sorted(copied)

if a.runtime_only:
    deploy_curl(binary);print('Gateway runtime dependencies prepared');raise SystemExit(0)

status=subprocess.check_output(['git','-C',str(source),'status','--porcelain'],text=True)
if status.strip():raise SystemExit('Commit the reviewed source before creating a release snapshot.')
commit=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
gateway=out/'网关';gateway.mkdir(exist_ok=True);(gateway/'config').mkdir(exist_ok=True)
libraries=deploy_curl(gateway)
for name in ('gateway.exe','mock_provider.exe'):shutil.copy2(binary/name,gateway/name)
canonical=gateway/'config/models.json'
if not canonical.exists():shutil.copy2(source/'config/models.providers.example.json',canonical)
shutil.copy2(source/'config/models.providers.example.json',gateway/'config/models.providers.example.json')
shutil.copy2(source/'config/models.example.json',gateway/'config/models.demo.example.json')
for origin,name in [(source/'integration/abcd/start-multi-model-gateway.ps1','启动网关.ps1'),
                    (source/'integration/release/demo.ps1','故障演示.ps1')]:
    (gateway/name).write_text(origin.read_text(encoding='utf-8-sig'),encoding='utf-8-sig')
(out/'工具').mkdir(exist_ok=True)
(out/'工具/检查交付.ps1').write_text((source/'integration/release/verify-delivery.ps1').read_text(encoding='utf-8-sig'),encoding='utf-8-sig')

# Use the same client deployment helper as its separately delivered package.
client=out/'客户端'
subprocess.run([sys.executable,str(source/'client/tools/package_windows.py'),'--build',str(a.build.resolve()),
    '--output',str(client),'--qt-bin',str(a.qt_bin),'--compiler-bin',str(a.compiler_bin)],check=True,capture_output=True)
for suffix in ('源码','运行包'):
    generated=out/('AI Gateway客户端V1.0_'+suffix+'.zip')
    destination=out.parent/(out.name+'_客户端'+suffix+'.zip')
    assert generated.resolve().parent==out and destination.resolve().parent==out.parent
    generated.replace(destination)
# The helper also produces standalone client archives, which are useful optional downloads.
def cmd(name,command):
    (out/name).write_bytes(('@echo off\r\nchcp 65001 >nul\r\ncd /d "%~dp0"\r\n'+command+'\r\n').encode('utf-8'))
cmd('01-启动真实网关.cmd','powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0网关\\启动网关.ps1"\r\npause')
cmd('02-打开客户端.cmd','start "" "%~dp0客户端\\AIGatewayClient.exe"')
for label,scenario in [('03-模拟正常演示.cmd','normal'),('04-模拟备用切换.cmd','fallback'),('05-模拟全部失败.cmd','all-fail')]:
    cmd(label,'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0网关\\故障演示.ps1" -Scenario '+scenario+'\r\npause')
cmd('06-检查交付包.cmd','powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0工具\\检查交付.ps1"\r\npause')
shutil.copy2(source/'docs/A交付说明.md',out/'使用说明.md')
(out/'先看这里.txt').write_text('''AI Gateway 最终交付包

1. 先在旧网关窗口按 Ctrl+C 关闭旧网关。
2. 双击“01-启动真实网关.cmd”，按提示填写自己的模型密钥；不填写的平台本次不显示。
3. 看到 Gateway ready 后保持窗口打开，再双击“02-打开客户端.cmd”。
4. 下次启动仍用这个目录。网关长期设置在“网关/config/models.json”，不会因为跳过密钥而被覆盖。

模拟演示：先关闭真实网关，再选择 03/04/05，打开客户端刷新模型后提问“解释 C++ 指针”。模拟回答会明确标注，演示不消耗模型额度。
06 用于检查程序文件与交付版本是否一致，不发送聊天请求。

密钥仅存在本次网关进程，不在包内、配置或日志中保存。历史只保存于各自电脑；当前为单次提问模式。
完整源码和版本说明见同一交付目录的源码压缩包。详细限制和验收见“使用说明.md”。
''',encoding='utf-8-sig')
licenses=gateway/'第三方许可';licenses.mkdir(exist_ok=True)
for origin,name in [(source/'ai-gateway/third_party/cpp-httplib/LICENSE','cpp-httplib.txt'),
                    (source/'ai-gateway/third_party/nlohmann/LICENSE.MIT','nlohmann-json.txt')]:shutil.copy2(origin,licenses/name)
curl_licenses=a.curl_bin.parent/'share/licenses'
if curl_licenses.exists():shutil.copytree(curl_licenses,licenses/'curl-runtime',dirs_exist_ok=True)

mutable={'网关/config/models.json','网关/config/models.available.json','网关/config/models.demo.runtime.json'}
entries=[]
for file in sorted(out.rglob('*')):
    if not file.is_file():continue
    name=file.relative_to(out).as_posix()
    if name in mutable or name=='交付清单.json' or '/logs/' in '/'+name or name.endswith('.zip'):continue
    entries.append({'path':name,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
manifest={'version':'1.0','source_commit':commit,'D_source_commit':'20561835449a31697d9de68c14cd16769fd0e9ea',
          'gateway_revision':subprocess.check_output([str(gateway/'gateway.exe'),'--version'],text=True).strip(),
          'runtime_libraries':libraries,'contains_credentials':False,'files':entries}
(out/'交付清单.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
source_zip=out.parent/(out.name+'_源码.zip')
subprocess.run(['git','-C',str(source),'archive','--format=zip','--prefix=源码/','-o',str(source_zip),'HEAD'],check=True)
with zipfile.ZipFile(source_zip,'a',zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('源码/release-info.json',json.dumps({'source_commit':commit},indent=2))
runtime_zip=out.parent/(out.name+'_运行包.zip')
with zipfile.ZipFile(runtime_zip,'w',zipfile.ZIP_DEFLATED) as archive:
    for file in out.rglob('*'):
        if file.is_file() and '/logs/' not in '/'+file.relative_to(out).as_posix():archive.write(file,out.name+'/'+file.relative_to(out).as_posix())
patch=subprocess.check_output(['git','-C',str(source),'diff','--binary','origin/main','HEAD','--',
    'gateway','integration/abcd','integration/release','docs/A交付说明.md'])
(out.parent/'A网关交付修改.patch').write_bytes(patch)
print(json.dumps({'directory':str(out),'commit':commit,'runtime_zip':str(runtime_zip),'source_zip':str(source_zip)},ensure_ascii=False))
