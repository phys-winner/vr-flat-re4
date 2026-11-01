"""Build a temporary shell DEX/JAR and check arguments; never contacts a device."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile


def build(output):
    root = Path(__file__).resolve().parents[2]
    sdk = Path(os.environ["ANDROID_SDK_ROOT"])
    java = Path(os.environ["JAVA_HOME"]) / "bin/java.exe"
    platform = sdk / "platforms/android-36/android.jar"
    d8 = Path(os.environ.get("ANDROID_BUILD_TOOLS_ROOT", sdk / "build-tools/36.0.0")) / "d8.bat"
    source = root / "analysis/research/RE4TouchInject.java"
    assert source.is_file() and java.is_file() and platform.is_file() and d8.is_file()
    assert not output.exists(), "refusing to replace an existing touch injector"
    with tempfile.TemporaryDirectory(prefix="re4-touch-inject-") as temporary:
        work = Path(temporary)
        classes, dex = work / "classes", work / "dex"
        classes.mkdir(); dex.mkdir()
        subprocess.run([str(java.with_name("javac.exe")), "-source", "8", "-target", "8", "-encoding", "UTF-8",
                        "-classpath", str(platform), "-d", str(classes), str(source)], check=True)
        command = [str(java), "-cp", str(classes) + os.pathsep + str(platform), "RE4TouchInject", "--check", "--display", "0", "--size", "2400", "1080"]
        subprocess.run(command + ["down:0,200,300", "down:1,1700,650", "down:2,1200,500", "move:2,1400,450",
                                 "wait:200", "up:0", "up:2", "up:1", "down:9,0,0", "cancel"], check=True)
        for steps in (["up:0"], ["move:0,1,1"], ["down:0,1,1", "down:0,2,2"], ["down:0,2400,0"],
                      ["down:0,NaN,0"], ["down:0,Infinity,0"], ["wait:60000", "wait:1"], ["down:10,0,0"], ["cancel:x"]):
            failed = subprocess.run(command + steps, capture_output=True)
            assert failed.returncode != 0, steps
        subprocess.run([str(d8), "--min-api", "23", "--lib", str(platform), "--output", str(dex),
                        *[str(path) for path in sorted(classes.glob("*.class"))]], check=True)
        data = (dex / "classes.dex").read_bytes()
        assert data.startswith(b"dex\n")
        output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_STORED) as archive:
            archive.writestr("classes.dex", data)
    receipt = {"source": str(source), "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
               "jar": str(output.resolve()), "jar_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
               "platform": str(platform), "d8": str(d8), "argument_checks": "PASS"}
    output.with_suffix(".json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf8")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    build(parser.parse_args().output.resolve())
