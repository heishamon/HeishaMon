"""Host test of the webserver (HeishaMon/src/common/webserver.cpp).

Compiles the real webserver with g++ and small stubs (stubs/unittest.h),
once with the ESP8266 buffer size and once with the ESP32 one, and runs
test_webserver.cpp against it. No device or network is used.

Run: python3 tests/webserver/run_tests.py
"""

import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = HERE / "build"
OUT.mkdir(parents=True, exist_ok=True)
COMMON = ROOT / "HeishaMon" / "src" / "common"

compiler = shutil.which("g++")
if not compiler:
    raise SystemExit("g++ is required")

sources = [HERE / "test_webserver.cpp"] + [
    COMMON / name
    for name in ("webserver.cpp", "strnstr.cpp", "strncasestr.cpp", "base64.cpp", "sha1.cpp")
]

# WEBSERVER_BUFFER_SIZE of the ESP8266 and of the ESP32 (webserver.h)
for size in (128, 512):
    executable = OUT / f"test_webserver_{size}"
    subprocess.run(
        [compiler, "-std=c++17", "-g", "-O1", "-Wall", "-Wno-write-strings", "-Wno-unused-function", "-Wno-register",
         "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
         f"-DWEBSERVER_BUFFER_SIZE={size}", "-I" + str(HERE / "stubs"), "-I" + str(COMMON)]
        + [str(s) for s in sources] + ["-o", str(executable)],
        check=True,
    )
    subprocess.run([str(executable)], check=True)

print("PASS: webserver host tests")
