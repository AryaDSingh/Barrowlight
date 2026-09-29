"""Run built Debug tests in isolated directories, preserving player saves/profiles."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
output = root / "build" / "playtest-pass"
output.mkdir(parents=True, exist_ok=True)
results = []
executables = sorted((root / "build" / "bin" / "Debug").glob("*_test.exe"))
if not executables:
    raise SystemExit("Build the Debug test targets first.")
for executable in executables:
    work = output / executable.stem
    work.mkdir(exist_ok=True)
    if executable.stem.startswith("application_"):
        shutil.copytree(root / "assets", work / "assets", dirs_exist_ok=True)
    try:
        result = subprocess.run(
            [str(executable)], cwd=work, capture_output=True, text=True, timeout=55
        )
        log = result.stdout + result.stderr
        code = result.returncode
    except subprocess.TimeoutExpired as error:
        log = str(error)
        code = "TIMEOUT"
    (work / "output.log").write_text(log, encoding="utf-8")
    entry = {
        "test": executable.stem,
        "exit": code,
        "checks": log.count("[ok]") + log.count("[FAIL]"),
    }
    results.append(entry)
    print(f"{executable.stem}: {code} ({entry['checks']} checks)", flush=True)
    for line in log.splitlines():
        if "[FAIL]" in line:
            print("  " + line, flush=True)
(output / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
failed = sum(entry["exit"] != 0 for entry in results)
print(f"{len(results)} executables, {failed} failures.")
sys.exit(bool(failed))
