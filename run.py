#!/usr/bin/env python
import os
import subprocess
import shutil
import tempfile
from sys import stdout
from typing import List


if not os.path.exists("build/"):
    os.mkdir("build")
else:
    print("the build directory is already there")

def run_command(cmd: List[str], stdin=None, stdout=None, capture_output=False) -> bytes:
    try:
        print(f"Running: {" ".join(cmd)}")
        proc = subprocess.run(
            cmd,
            check=True,
            stdin=stdin,
            stdout=stdout,
            capture_output=capture_output
        )
        return proc.stdout
    except subprocess.CalledProcessError as e:
        exit(e.returncode)

os.chdir("build/")
run_command(["cmake", ".."])
run_command(["make", "-j", "4"])
run_command(["./haste"])
with open("out.json", "rb") as f:
    v = run_command(["node", "../fix-json.js"], stdin=f, capture_output=True)

with tempfile.NamedTemporaryFile("wb", delete=False) as tmp:
    tmp.write(v)
    tmp_name = tmp.name
shutil.move(tmp_name, "out.json")

with open("out.json", "rb") as fl:
    run_command(["jq", "."], stdin=fl)

