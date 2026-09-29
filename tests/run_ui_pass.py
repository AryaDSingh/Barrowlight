"""Run the two built UI integration targets with isolated saves and screenshots."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
output = root / "build" / "ui-acceptance"
results = []
for name in ("application_targeting_test", "application_rewards_test"):
    executable = root / "build" / "bin" / "Debug" / f"{name}.exe"
    if not executable.exists():
        raise SystemExit(f"Build {name} first.")
    work = output / name
    work.mkdir(parents=True, exist_ok=True)
    shutil.copytree(root / "assets", work / "assets", dirs_exist_ok=True)
    try:
        result = subprocess.run([str(executable)], cwd=work, capture_output=True,
                                text=True, timeout=90)
        log = result.stdout + result.stderr
        code = result.returncode
    except subprocess.TimeoutExpired as error:
        log = str(error)
        code = "TIMEOUT"
    (work / "output.log").write_text(log, encoding="utf-8")
    entry = {"test": name, "exit": code,
             "checks": log.count("[ok]") + log.count("[FAIL]")}
    results.append(entry)
    print(f"{name}: exit {code}, {entry['checks']} checks", flush=True)
    for line in log.splitlines():
        if "[FAIL]" in line:
            print(line, flush=True)
(output / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
sys.exit(any(entry["exit"] != 0 for entry in results))
