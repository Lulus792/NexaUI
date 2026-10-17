"""Run bounded libFuzzer campaigns on fresh copies of repository seeds."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
TARGETS = {"sfnt": 512, "gsub": 4096, "gpos": 2048, "text": 512, "layout": 256}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_campaign(build_dir, output_dir, targets, runs, seed, wall_timeout,
                 corpus_root=ROOT / "fuzz" / "corpus"):
    build_dir, output_dir, corpus_root = (
        Path(path).resolve() for path in (build_dir, output_dir, corpus_root))
    if output_dir.is_relative_to(corpus_root) or corpus_root.is_relative_to(output_dir):
        raise ValueError("output directory must be outside the source corpus")
    suffix = ".exe" if os.name == "nt" else ""
    executables = {name: build_dir / f"nexaui_{name}_fuzz{suffix}" for name in targets}
    for name, executable in executables.items():
        if not executable.is_file():
            raise ValueError(f"missing fuzzer: {executable}")
        if not (corpus_root / name).is_dir():
            raise ValueError(f"missing seed directory: {corpus_root / name}")
    # Refuse reuse: accumulated discoveries would change the starting corpus.
    output_dir.mkdir(parents=True, exist_ok=False)
    summary = {"platform": platform.platform(), "runs": runs, "seed": seed,
               "wall_timeout_seconds": wall_timeout, "targets": []}
    environment = os.environ.copy()
    environment["UBSAN_OPTIONS"] = "halt_on_error=1"
    summary["sanitizer_environment"] = {
        key: environment.get(key) for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS")}
    failed = False
    for name, executable in executables.items():
        target_dir = output_dir / name
        corpus_dir = target_dir / "corpus"
        artifact_dir = target_dir / "artifacts"
        corpus_dir.mkdir(parents=True)
        artifact_dir.mkdir()
        seeds = {}
        for source in sorted((corpus_root / name).iterdir()):
            if source.is_symlink() or not source.is_file():
                raise ValueError(f"seed must be a regular file: {source}")
            seeds[source.name] = digest(source)
            shutil.copyfile(source, corpus_dir / source.name)
        command = [str(executable), str(corpus_dir), f"-seed={seed}",
                   f"-runs={runs}", f"-max_len={TARGETS[name]}", "-timeout=10",
                   f"-artifact_prefix={artifact_dir.as_posix()}/"]
        start = time.monotonic()
        log_path = target_dir / "fuzzer.log"
        with log_path.open("wb") as log:
            try:
                process = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                         env=environment, timeout=wall_timeout, check=False)
                code = process.returncode
            except subprocess.TimeoutExpired:
                code = 124
                log.write(b"\nCampaign exceeded wall-clock timeout.\n")
            except OSError as error:
                code = 127
                log.write(f"\nCannot run fuzzer: {error}\n".encode())
        log_text = log_path.read_text(encoding="utf-8", errors="replace")
        counters = re.findall(r"DONE\s+cov:\s*(\d+)", log_text)
        summary["targets"].append({
            "name": name, "command": command, "executable_sha256": digest(executable),
            "initial_seed_sha256": seeds, "exit_code": code,
            "elapsed_seconds": round(time.monotonic() - start, 3),
            "final_coverage_counters": int(counters[-1]) if counters else None,
            "log": str(log_path), "artifacts": str(artifact_dir)})
        (output_dir / "summary.json").write_text(
            json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        print(f"{name}: exit={code}, log={log_path}", flush=True)
        failed |= code != 0
    return 1 if failed else 0


def positive(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--target", choices=["all", *TARGETS], default="all")
    parser.add_argument("--runs", type=positive, default=10000)
    parser.add_argument("--seed", type=int, default=1800)
    parser.add_argument("--wall-timeout", type=positive, default=300)
    args = parser.parse_args()
    if not 0 <= args.seed <= 0xffffffff:
        parser.error("seed must be between 0 and 4294967295")
    targets = list(TARGETS) if args.target == "all" else [args.target]
    try:
        return run_campaign(args.build_dir, args.output_dir, targets,
                            args.runs, args.seed, args.wall_timeout)
    except (ValueError, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    raise SystemExit(main())
