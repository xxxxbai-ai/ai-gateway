"""Publish prepared local artifacts to the original V1 release using Git's existing login.

Run on the normal Windows desktop, where the user's Git credential manager and
network are available. Credentials remain in memory and are never printed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import urllib.parse
import urllib.request

SOURCE = Path(__file__).resolve().parents[2]
REPO = "xxxxbai-ai/ai-gateway"
TAG = "v1.0.0-rc.1"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts", required=True, type=Path)
    args = parser.parse_args()
    artifacts = args.artifacts.resolve()
    head = subprocess.check_output(["git", "-C", str(SOURCE), "rev-parse", "HEAD"], text=True).strip()
    evidence = json.loads((artifacts / "verification.json").read_text(encoding="utf-8"))
    assert evidence["source_commit"] == head, "Local source changed; rebuild the prepared artifacts."
    names = [f"AI-Gateway-{TAG}-source.zip", f"AI-Gateway-{TAG}-windows-x64.zip",
             "SHA256SUMS.txt", "verification.json"]
    data = {name: (artifacts / name).read_bytes() for name in names}
    digest = {name: "sha256:" + hashlib.sha256(value).hexdigest() for name, value in data.items()}
    checks = dict((line.split()[1], line.split()[0])
                  for line in data["SHA256SUMS.txt"].decode().splitlines() if line.strip())
    for name, expected in checks.items():
        assert digest[name] == "sha256:" + expected, "Prepared archive checksum mismatch."
    env = dict(os.environ, GIT_TERMINAL_PROMPT="0", GCM_INTERACTIVE="Never")
    credential = subprocess.run(["git", "credential", "fill"], cwd=SOURCE,
        input=b"url=https://github.com/xxxxbai-ai/ai-gateway.git\n\n", capture_output=True, env=env, timeout=30)
    values = dict(line.split("=", 1) for line in credential.stdout.decode().splitlines() if "=" in line)
    if credential.returncode or not values.get("password"):
        raise SystemExit("GitHub login is unavailable. Sign in with your usual Git credential manager, then retry.")
    headers = {"Authorization": "Bearer " + values["password"],
               "Accept": "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28",
               "User-Agent": "AI-Gateway-Local-Release-Sync"}

    def api(method, path, value=None, binary=None):
        url = path if path.startswith("https://") else "https://api.github.com" + path
        assert urllib.parse.urlparse(url).hostname in ("api.github.com", "uploads.github.com")
        body = json.dumps(value, ensure_ascii=False).encode() if value is not None else binary
        request = urllib.request.Request(url, data=body, method=method, headers={**headers,
            "Content-Type": "application/json" if binary is None else "application/octet-stream"})
        with urllib.request.urlopen(request, timeout=180) as response:
            return json.load(response) if response.status != 204 else None

    repo = f"/repos/{REPO}"
    allowed = (evidence["base_source_commit"], head)
    assert api("GET", repo + "/commits/main")["sha"] in allowed, "Remote main changed; review before syncing."
    release = api("GET", repo + "/releases/tags/" + TAG)
    assert release["id"] == 438025409 and not release.get("immutable")
    ref = api("GET", repo + "/git/ref/tags/" + TAG)
    assert ref["object"]["type"] == "commit" and ref["object"]["sha"] in allowed
    subprocess.run(["git", "-C", str(SOURCE), "push", "origin", "HEAD:refs/heads/main"], check=True, env=env)
    subprocess.run(["git", "-C", str(SOURCE), "push", "origin",
                    "HEAD:refs/heads/codex/v1-team-validation-20261108"], check=True, env=env)
    existing = {item["name"]: item for item in release["assets"]}
    staged = []
    for name in names:
        pending = name + ".pending-" + head[:12]
        item = existing.get(pending)
        if item is None:
            url = release["upload_url"].split("{", 1)[0] + "?" + urllib.parse.urlencode({"name": pending})
            item = api("POST", url, binary=data[name])
        assert item["state"] == "uploaded" and item["digest"] == digest[name]
        staged.append((name, item))
        print("Uploaded and verified: " + name, flush=True)
    assert api("GET", repo + "/commits/main")["sha"] == head
    current_ref = api("GET", repo + "/git/ref/tags/" + TAG)
    assert current_ref["object"]["sha"] == ref["object"]["sha"]
    for name, item in staged:
        if name in existing:
            api("DELETE", existing[name]["url"])
        api("PATCH", item["url"], {"name": name})
    api("PATCH", repo + "/git/refs/tags/" + TAG, {"sha": head, "force": True})
    body = release["body"].replace(evidence["base_source_commit"], head)
    body = body.replace("完整工程 25 组测试通过，100 条补充分类型问题通过打包网关检查。",
                        "原交付版完整工程 25 组测试通过，100 条补充分类型问题通过打包网关检查。")
    notice = ("2026-11-08 已修复中文输入法拼音与灰色提示语重叠。"
              "本次输入法、地址校验、历史恢复及 Markdown 验证通过；当前受限环境未通过完整网络联调，"
              "网关程序沿用原版。\n\n")
    api("PATCH", release["url"], {"target_commitish": head, "body": notice + body})
    final = api("GET", release["url"])
    final_assets = {item["name"]: item for item in final["assets"]}
    for name in names:
        assert final_assets[name]["digest"] == digest[name]
    result = {"url": final["html_url"], "source_commit": head, "status": "updated",
              "assets": [{"name": name, "digest": digest[name]} for name in names]}
    (artifacts / "last-sync-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("Updated the original GitHub release: " + final["html_url"], flush=True)


if __name__ == "__main__":
    main()
