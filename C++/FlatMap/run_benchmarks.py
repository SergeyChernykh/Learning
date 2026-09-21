#!/usr/bin/env python3
"""Compile, validate and compare both implementations without changing headers."""
import argparse
import csv
import io
import os
from pathlib import Path
import platform
import statistics
import subprocess
import hashlib

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--output", type=Path, default=ROOT / "benchmark_results")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cxx = os.environ.get("CXX", "g++")
    flags = ["-std=c++20", "-O3", "-DNDEBUG", "-Wall", "-Wextra", "-Wpedantic"]
    variants = {"pairs": "FlatMap.h", "split": "FlatMap2.h"}
    binaries = {}
    if hasattr(os, "sched_getaffinity"):
        cpu_id = min(os.sched_getaffinity(0))
        os.sched_setaffinity(0, {cpu_id})
        print(f"Pinned to logical CPU {cpu_id}", flush=True)
    else:
        cpu_id = "not pinned"
    for name, header in variants.items():
        define = f'-DFLATMAP_HEADER="{header}"'
        test = out / f"test_{name}"
        subprocess.run([cxx, "-std=c++20", "-g", "-D_GLIBCXX_DEBUG",
                        "-fsanitize=address,undefined", define, "review_tests.cpp", "-o", str(test)],
                       cwd=ROOT, check=True)
        subprocess.run([str(test)], check=True)
        checked = out / f"checked_{name}"
        subprocess.run([cxx, "-std=c++20", "-O1", "-g", "-D_GLIBCXX_DEBUG",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", define,
                        "benchmark.cpp", "-o", str(checked)], cwd=ROOT, check=True)
        subprocess.run([str(checked), "--verify"], check=True)
        binaries[name] = out / f"bench_{name}"
        subprocess.run([cxx, *flags, define, "benchmark.cpp", "-o", str(binaries[name])],
                       cwd=ROOT, check=True)

    def run(name):
        result = subprocess.run([str(binaries[name])], check=True, text=True, capture_output=True)
        return list(csv.DictReader(io.StringIO(result.stdout)))

    for name in variants:
        run(name)  # Discard one complete warmup run per variant.
    rows = []
    for trial in range(args.runs):
        names = list(variants) if trial % 2 == 0 else list(reversed(variants))
        for name in names:
            print(f"Run {trial + 1}/{args.runs}: {name}", flush=True)
            rows.extend(dict(variant=name, trial=trial + 1, **row) for row in run(name))
    with (out / "raw.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    lines = ["| Key | Value | N | Operation | Pairs ns/op | Split ns/op | Pairs / split |",
             "|:---|:---|---:|:---|---:|---:|---:|"]
    keys = sorted({(r["key_type"], r["value_type"], int(r["size"]), r["operation"]) for r in rows})
    for key, value, n, op in keys:
        medians = {name: statistics.median(float(r["ns_per_op"]) for r in rows
                   if r["variant"] == name and r["key_type"] == key
                   and r["value_type"] == value and int(r["size"]) == n
                   and r["operation"] == op) for name in variants}
        a, b = medians["pairs"], medians["split"]
        lines.append(f"| {key} | {value} | {n} | {op} | {a:.2f} | {b:.2f} | {a/b:.2f} |")
    compiler = subprocess.check_output([cxx, "--version"], text=True).splitlines()[0]
    cpu = "unknown"
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.exists():
        cpu = next((line.split(":", 1)[1].strip() for line in cpuinfo.read_text().splitlines()
                    if line.startswith("model name")), cpu)
    metadata = f"CPU: {cpu}\nOS: {platform.platform()}\nCompiler: {compiler}\nFlags: {' '.join(flags)}\nRuns: {args.runs}\n"
    metadata += f"Logical CPU: {cpu_id}\n"
    for filename in ["FlatMap.h", "FlatMap2.h", "benchmark.cpp", "run_benchmarks.py"]:
        metadata += f"SHA256 {filename}: {hashlib.sha256((ROOT / filename).read_bytes()).hexdigest()}\n"
    metadata += "Object sizes (key, value): " + repr(sorted({(r["key_type"], r["value_type"], r["key_bytes"], r["value_bytes"]) for r in rows})) + "\n"
    (out / "summary.md").write_text(metadata + "\n" + "\n".join(lines) + "\n")
    print(metadata + "\n" + "\n".join(lines))


if __name__ == "__main__":
    main()
