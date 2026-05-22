#!/usr/bin/env python3
import os, subprocess, sys, glob, re

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HASTE = os.path.join(PROJECT_DIR, "haste")


def green(s):
    print(f"\033[32m{s}\033[0m")


def red(s):
    print(f"\033[31m{s}\033[0m")


def _test_path(group_dir, name, suffix):
    return os.path.join(PROJECT_DIR, group_dir, f"{name}.{suffix}")


def _run_one_test(group, file_path, index=0, leng=0):
    name = os.path.splitext(os.path.basename(file_path))[0]
    group_dir = group["dir"]
    got_path = _test_path(group_dir, name, group["got_suffix"])
    expected_path = _test_path(group_dir, name, group["expected_suffix"])
    kind = group["kind"]
    cmd = [HASTE, *group["flags"], file_path]
    skip = group.get("skip_lines", 0)
    expect_failure = group.get("expect_failure", False)
    file_output_ext = group.get("file_output")

    result = subprocess.run(cmd, capture_output=True, cwd=PROJECT_DIR)

    if file_output_ext:
        output_path = os.path.splitext(file_path)[0] + file_output_ext
        with open(output_path, "rb") as f:
            got_data = f.read()
    else:
        ansi_re = re.compile(rb'\033\[[0-9;]*[a-zA-Z]')
        def strip_ansi(data):
            return ansi_re.sub(b'', data)

        if expect_failure:
            got_data = strip_ansi(result.stderr) if result.stderr else b''
        else:
            got_data = result.stdout
            if result.stderr:
                got_data += result.stderr

    with open(got_path, "wb") as f:
        f.write(got_data)

    if expect_failure:
        if result.returncode == 0:
            red(f"{int((float(index) / float(leng)) * 100.0):3}% FAIL: {kind:>10}: {name} (expected error but compiled successfully)")
            return {"name": name, "kind": kind, "passed": False}
    else:
        if result.returncode != 0:
            red(f"{int((float(index) / float(leng)) * 100.0):3}% FAIL: {kind:>10}: {name} (compiler crash)")
            return {"name": name, "kind": kind, "passed": False}

    with open(expected_path) as f:
        expected_lines = f.readlines()[skip:]
    with open(got_path) as f:
        got_lines = f.readlines()[skip:]

    if expected_lines == got_lines:
        green(f"{int((float(index) / float(leng)) * 100.0):3}% PASS: {kind:>10}: {name}")
        return {"name": name, "kind": kind, "passed": True}
    else:
        red(f"{int((float(index) / float(leng)) * 100.0):3}% FAIL {kind:>10}: {name} (output mismatch)")
        subprocess.run(["diff", "-u", expected_path, got_path])
        return {"name": name, "kind": kind, "passed": False}


def _discover_tests(group):
    pattern = os.path.join(PROJECT_DIR, group["dir"], group["pattern"])
    return sorted(glob.glob(pattern))


# ── Configure test groups here ──────────────────────────────────
TEST_GROUPS = [
    {
        "name": "lexing",
        "kind": "tokens",
        "dir": "test/lexing",
        "pattern": "*.haste",
        "flags": ["--tokens", "--no-fun"],
        "expected_suffix": "tokens.expected",
        "got_suffix": "tokens.got",
        "file_output": ".tokens",
    },
    {
        "name": "errors",
        "kind": "errors",
        "dir": "test/errors",
        "pattern": "*.haste",
        "flags": ["--no-fun"],
        "expected_suffix": "err.expected",
        "got_suffix": "err.got",
        "expect_failure": True,
    },
    {
        "name": "integration",
        "kind": "llvm",
        "dir": "test/integration",
        "pattern": "*.haste",
        "flags": ["--llvm", "--no-fun"],
        "expected_suffix": "expected",
        "got_suffix": "got",
        "skip_lines": 2,
        "file_output": ".ll",
    },
]
# ────────────────────────────────────────────────────────────────


def main():
    all_tests = []
    for group in TEST_GROUPS:
        for file in _discover_tests(group):
            all_tests.append((group, file))

    if not all_tests:
        red("No tests found!")
        sys.exit(1)

    passed = 0
    failed = 0

    i = 0
    for group, file in all_tests:
        r = _run_one_test(group, file, i, len(all_tests))
        if r["passed"]:
            passed += 1
        else:
            failed += 1
        i += 1

    print("---")
    if failed == 0:
        green(f"All tests passed! 100% ({len(all_tests)} tests in total)")
    else:
        red(f"{failed} test(s) failed ({int((float(failed) / float(len(all_tests))) * 100.0)}%), {passed} passed ({int((float(passed) / float(len(all_tests))) * 100.0)}%)")
    sys.exit(failed)


if __name__ == "__main__":
    main()
