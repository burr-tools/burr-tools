#!/usr/bin/env python3
"""Compare two burrTools benchmark CSVs (bench_solve.py output) as Markdown.

Usage:
    bench/compare_snapshots.py BEFORE.csv AFTER.csv [--tag1 TAG] [--tag2 TAG]

Reads the data rows of two result files (skipping '#' provenance lines and
any appended summary tables), takes per-puzzle medians across runs, and
prints speed (wall time) and peak-RSS memory comparison tables as Markdown
to stdout. Files that contain a single data tag (e.g. `just bench`
snapshots, tagged `single`) need no extra flags; files with several tags
(e.g. A/B runs) select them via --tag1/--tag2.
"""
import argparse
import statistics
import sys


def parse_file(path):
    """Return (provenance dict, {tag: {puzzle: [row dicts]}})."""
    provenance = {}
    data = {}
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            if line.startswith("#"):
                if ":" in line:
                    key, _, value = line[1:].partition(":")
                    provenance[key.strip()] = value.strip()
                continue
            parts = line.split(",", 8)
            if len(parts) != 9:
                continue  # summary tables / separators
            tag, puzzle, run, wall, user, sys_, cpu, rss, rest = parts
            if tag == "tag":
                continue  # header row
            try:
                exit_code, _, _stats = rest.partition(",")
                row = {
                    "wall": float(wall),
                    "rss": float(rss.rstrip("MB")),
                    "exit": int(exit_code),
                }
            except ValueError:
                continue  # not a data row
            data.setdefault(tag, {}).setdefault(puzzle, []).append(row)
    return provenance, data


def pick_tag(data, path, explicit):
    tags = sorted(data)
    if explicit:
        if explicit not in data:
            sys.exit(f"error: tag '{explicit}' not found in {path} (have: {tags})")
        return explicit
    if len(tags) == 1:
        return tags[0]
    sys.exit(f"error: {path} has several data tags {tags}; pass --tag explicitly")


def short_name(puzzle):
    return puzzle.replace("puzzles/BTFiles/", "").replace("examples/", "")


def describe(path, provenance, tag, puzzles):
    """One-line source description for the report header."""
    commit = provenance.get("git_commit", "?")
    if len(commit) > 12:
        commit = commit[:12]
    subject = provenance.get("git_subject", "?")
    stamp = provenance.get("timestamp_utc", "?")
    host = provenance.get("host", "?")
    runs = sorted({len(v) for v in puzzles.values()})
    return (f"`{path}` — commit `{commit}` ({subject}), {stamp}, "
            f"host {host}, tag `{tag}`, runs per puzzle: {runs}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("before_csv")
    ap.add_argument("after_csv")
    ap.add_argument("--tag1", default=None, help="data tag to use from BEFORE file")
    ap.add_argument("--tag2", default=None, help="data tag to use from AFTER file")
    args = ap.parse_args()

    prov1, data1 = parse_file(args.before_csv)
    prov2, data2 = parse_file(args.after_csv)
    tag1 = pick_tag(data1, args.before_csv, args.tag1)
    tag2 = pick_tag(data2, args.after_csv, args.tag2)
    d1, d2 = data1[tag1], data2[tag2]

    common = sorted(set(d1) & set(d2))
    if not common:
        sys.exit("error: no puzzles in common between the two files")
    only1 = sorted(set(d1) - set(d2))
    only2 = sorted(set(d2) - set(d1))

    print("# Benchmark comparison")
    print()
    print(f"- Before: {describe(args.before_csv, prov1, tag1, d1)}")
    print(f"- After: {describe(args.after_csv, prov2, tag2, d2)}")
    if only1:
        print(f"- Only in before: {', '.join(short_name(p) for p in only1)}")
    if only2:
        print(f"- Only in after: {', '.join(short_name(p) for p in only2)}")
    print()
    print("Medians across runs per puzzle. Speedup > 1 means after is faster.")
    print()
    print("## Speed (median wall time)")
    print()
    print(f"{'Puzzle':<55} {'Before (s)':>10} {'After (s)':>10} {'Speedup':>8}")
    print("-" * 86)
    for puzzle in common:
        med_b = statistics.median(r["wall"] for r in d1[puzzle])
        med_a = statistics.median(r["wall"] for r in d2[puzzle])
        speedup = med_b / med_a if med_a > 0 else float("inf")
        print(f"{short_name(puzzle):<55} {med_b:>10.2f} {med_a:>10.2f} {speedup:>7.2f}x")
    print()
    print("## Memory (median peak RSS)")
    print()
    print(f"{'Puzzle':<55} {'Before':>10} {'After':>10} {'Delta':>12}")
    print("-" * 90)
    for puzzle in common:
        med_b = statistics.median(r["rss"] for r in d1[puzzle])
        med_a = statistics.median(r["rss"] for r in d2[puzzle])
        delta = med_a - med_b
        pct = (delta / med_b * 100.0) if med_b > 0 else 0.0
        print(f"{short_name(puzzle):<55} {med_b:>9.1f}M {med_a:>9.1f}M {delta:>+9.1f}M ({pct:>+5.1f}%)")


if __name__ == "__main__":
    main()
