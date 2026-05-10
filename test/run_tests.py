#!/usr/bin/env python3
import os, subprocess, sys, glob

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HASTE = os.path.join(PROJECT_DIR, "haste")


def green(s):
    print(f"\033[32m{s}\033[0m")


def red(s):
    print(f"\033[31m{s}\033[0m")


def _test_path(group_dir, name, suffix):
    return os.path.join(PROJECT_DIR, group_dir, f"{name}.{suffix}")


def _run_one_test(group, file_path):
    name = os.path.splitext(os.path.basename(file_path))[0]
    group_dir = group["dir"]
    got_path = _test_path(group_dir, name, group["got_suffix"])
    expected_path = _test_path(group_dir, name, group["expected_suffix"])
    kind = group["kind"]
    cmd = [HASTE, *group["flags"], file_path]
    skip = group.get("skip_lines", 0)

    result = subprocess.run(cmd, capture_output=True, cwd=PROJECT_DIR)
    with open(got_path, "wb") as f:
        f.write(result.stdout)
        if result.stderr:
            f.write(result.stderr)

    if result.returncode != 0:
        red(f"FAIL: {name} ({kind} — compiler crash)")
        return {"name": name, "kind": kind, "passed": False}

    with open(expected_path) as f:
        expected_lines = f.readlines()[skip:]
    with open(got_path) as f:
        got_lines = f.readlines()[skip:]

    if expected_lines == got_lines:
        green(f"PASS: {name} ({kind})")
        return {"name": name, "kind": kind, "passed": True}
    else:
        red(f"FAIL: {name} ({kind} — output mismatch)")
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
        "flags": ["--tokens"],
        "expected_suffix": "tokens.expected",
        "got_suffix": "tokens.got",
    },
    {
        "name": "integration",
        "kind": "llvm",
        "dir": "test/integration",
        "pattern": "*.haste",
        "flags": ["--llvm"],
        "expected_suffix": "expected",
        "got_suffix": "got",
        "skip_lines": 2,
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

    for group, file in all_tests:
        r = _run_one_test(group, file)
        if r["passed"]:
            passed += 1
        else:
            failed += 1

    print("---")
    if failed == 0:
        green("All tests passed!")
    else:
        red(f"{failed} test(s) failed, {passed} passed")
    sys.exit(failed)


if __name__ == "__main__":
    main()
