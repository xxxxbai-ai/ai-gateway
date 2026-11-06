"""Actual A/C + Windows PowerShell startup persistence. Fake credentials; no inference."""
import argparse, copy, http.client, json, os, re, subprocess, tempfile, time
from http.client import HTTPConnection
from pathlib import Path

p=argparse.ArgumentParser()
for key in ('gateway','script','config','work'): p.add_argument('--'+key,required=True)
args=p.parse_args()
Path(args.work).mkdir(parents=True,exist_ok=True)
shell=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
flags=getattr(subprocess,'CREATE_NO_WINDOW',0)
base=json.loads(Path(args.config).read_text(encoding='utf-8-sig'))

with tempfile.TemporaryDirectory(dir=args.work) as temporary:
    root=Path(temporary); config=root/'长期配置.json'; available=root/'本次可用.json'
    env=dict(os.environ); secrets=[]
    for i,m in enumerate(base['models']):
        key='A_STARTUP_FIXTURE_KEY_'+str(i);m['credential_ref']='env:'+key;env.pop(key,None)
        if m['enabled']:
            env[key]='FAKE_STARTUP_VALUE_'+str(i);secrets.append(env[key])
    config.write_text(json.dumps(base,ensure_ascii=False),encoding='utf-8')
    def prepare(environment):
        return subprocess.run([str(shell),'-NoProfile','-ExecutionPolicy','Bypass','-File',args.script,
            '-GatewayPath',args.gateway,'-TemplatePath',str(config),'-AvailableConfigPath',str(available),
            '-PrepareOnly','-NoPrompt'],env=environment,capture_output=True,timeout=20,creationflags=flags)
    result=prepare(env);assert result.returncode==0,result.stdout+result.stderr
    assert json.loads(config.read_text(encoding='utf-8'))==base
    assert not any(s.encode() in result.stdout+result.stderr+available.read_bytes() for s in secrets)
    print('PASS startup: credentials stay out of files and console; durable config unchanged')

    log=root/'requests.jsonl'; output=root/'gateway.txt'
    with output.open('wb') as stream:
        child=subprocess.Popen([args.gateway,'--port','0','--config',str(config),'--require-credentials','--log',str(log)],
            env=env,stdout=stream,stderr=subprocess.STDOUT,creationflags=flags)
    try:
        for _ in range(120):
            text=output.read_text(encoding='utf-8',errors='replace');match=re.search(r'Gateway ready: http://127\.0\.0\.1:(\d+)',text)
            if match: port=int(match.group(1));break
            assert child.poll() is None,text;time.sleep(.03)
        else: raise AssertionError('gateway startup timeout')
        def http(method,path,value=None):
            connection=HTTPConnection('127.0.0.1',port,timeout=5)
            try:
                connection.request(method,path,None if value is None else json.dumps(value).encode(),{'Content-Type':'application/json'})
                response=connection.getresponse();body=json.loads(response.read());assert response.status==200,body;return body
            finally: connection.close()
        settings=http('GET','/api/config')['models'];settings[0]['scores']['programming']=41;settings[1]['enabled']=False
        http('POST','/api/config',{'models':settings})
        assert 'qwen_plus' not in {m['id'] for m in http('GET','/api/models')['models']}
    finally:
        child.terminate();child.wait(timeout=5)
    saved=config.read_bytes();assert json.loads(saved)['models'][0]['scores']['programming']==41
    assert prepare(env).returncode==0
    assert config.read_bytes()==saved
    assert json.loads(available.read_text(encoding='utf-8'))['models'][0]['scores']['programming']==41
    print('PASS startup: actual C save survives next startup; disabled model stays disabled')
    partial=dict(env);partial.pop(next(m['credential_ref'][4:] for m in base['models'] if m['id']=='glm_flash'),None)
    assert prepare(partial).returncode==0 and config.read_bytes()==saved
    prepared=json.loads(available.read_text(encoding='utf-8'))
    assert next(m for m in prepared['models'] if m['id']=='glm_flash')['enabled'] is False
    assert next(m for m in json.loads(saved)['models'] if m['id']=='glm_flash')['enabled'] is True
    assert prepare(env).returncode==0
    assert next(m for m in json.loads(available.read_text(encoding='utf-8'))['models'] if m['id']=='glm_flash')['enabled'] is True
    print('PASS startup: missing credential only masks this run; next startup can restore it')
    empty=dict(env)
    for i in range(len(base['models'])): empty.pop('A_STARTUP_FIXTURE_KEY_'+str(i),None)
    assert prepare(empty).returncode!=0 and config.read_bytes()==saved
    assert not any(s in config.read_text(encoding='utf-8') for s in secrets)
    print('PASS startup: no credentials refuses launch without damaging saved settings')
