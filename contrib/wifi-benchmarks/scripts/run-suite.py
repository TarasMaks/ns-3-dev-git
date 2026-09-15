#!/usr/bin/env python3
#
# Copyright (c) 2026
#
# SPDX-License-Identifier: GPL-2.0-only
#
"""Run the wifi-benchmarks suite (Wi-Fi 6 / 7 / 8) and summarize the results.

Each benchmark is an ns-3 example of the wifi-benchmarks module that writes a CSV and a JSON
file with its measurements, the reference values from the standards or the literature and a
PASS / FAIL / INFO verdict per row. This driver runs the selected benchmarks (optionally in
parallel), collects the JSON files and writes summary.md, summary.json and (optionally) SVG
charts. Only the Python standard library is required.

Examples:
  python3 contrib/wifi-benchmarks/scripts/run-suite.py --profile quick
  python3 contrib/wifi-benchmarks/scripts/run-suite.py --generation wifi7 --jobs 2
  python3 contrib/wifi-benchmarks/scripts/run-suite.py --only "mlo|emlsr" --svg
"""

import argparse
import concurrent.futures
import datetime
import json
import os
import re
import shlex
import subprocess
import sys
import time

# (name, generation, example, arguments, description)
BENCHMARKS = [
    # ---- Wi-Fi 6 (802.11ax) ----
    ("wifi6-phy-rates", "wifi6", "wifi-bench-phy-rates", "--standard=ax",
     "HE PHY data rates vs IEEE 802.11ax-2021 tables; saturated single-user goodput vs the analytical bound"),
    ("wifi6-per-vs-snr", "wifi6", "wifi-bench-per-vs-snr", "--standard=ax",
     "PER vs SNR of the error models; SNR at 10% PER vs the receiver sensitivity of the standard"),
    ("wifi6-aggregation", "wifi6", "wifi-bench-aggregation", "--standard=ax",
     "A-MPDU / A-MSDU / Block Ack window (256) goodput vs the analytical bound"),
    ("wifi6-edca-bianchi", "wifi6", "wifi-bench-edca-bianchi", "--standard=ax",
     "EDCA saturation throughput vs the Bianchi model with A-MPDU aggregation"),
    ("wifi6-rate-adaptation", "wifi6", "wifi-bench-rate-adaptation", "--standard=ax",
     "Goodput vs distance with Ideal / MinstrelHt / ThompsonSampling vs a genie bound"),
    ("wifi6-dl-ofdma", "wifi6", "wifi-bench-dl-ofdma", "--standard=ax",
     "DL OFDMA vs SU: throughput vs the per-RU PHY bound, latency with small packets"),
    ("wifi6-ul-ofdma", "wifi6", "wifi-bench-ul-ofdma", "--standard=ax",
     "UL OFDMA (Basic / BSRP trigger frames, MU EDCA) vs EDCA contention (Bianchi)"),
    ("wifi6-spatial-reuse", "wifi6", "wifi-bench-spatial-reuse", "--standard=ax",
     "BSS coloring / OBSS-PD spatial reuse between two overlapping BSSs"),
    ("wifi6-power-save", "wifi6", "wifi-bench-power-save", "--standard=ax",
     "Legacy power save energy and latency (TWT proxy)"),
    # ---- Wi-Fi 7 (802.11be) ----
    ("wifi7-phy-rates", "wifi7", "wifi-bench-phy-rates", "--standard=be",
     "EHT PHY rates (320 MHz, 4096-QAM) vs IEEE 802.11be-2024 tables; single-user goodput bound"),
    ("wifi7-per-vs-snr", "wifi7", "wifi-bench-per-vs-snr", "--standard=be",
     "PER vs SNR for EHT MCS 0-13 vs the receiver sensitivity of the standard"),
    ("wifi7-aggregation", "wifi7", "wifi-bench-aggregation", "--standard=be",
     "A-MPDU up to 15.5 MB and Block Ack window 1024 goodput vs the analytical bound"),
    ("wifi7-edca-bianchi", "wifi7", "wifi-bench-edca-bianchi", "--standard=be",
     "EDCA saturation throughput of EHT stations vs the Bianchi model"),
    ("wifi7-rate-adaptation", "wifi7", "wifi-bench-rate-adaptation", "--standard=be",
     "Goodput vs distance with rate adaptation for EHT"),
    ("wifi7-dl-ofdma", "wifi7", "wifi-bench-dl-ofdma", "--standard=be --channelWidth=80",
     "DL OFDMA with EHT at 80 MHz vs SU"),
    ("wifi7-ul-ofdma", "wifi7", "wifi-bench-ul-ofdma", "--standard=be",
     "UL OFDMA with EHT vs EDCA contention"),
    ("wifi7-spatial-reuse", "wifi7", "wifi-bench-spatial-reuse", "--standard=be",
     "OBSS-PD spatial reuse with EHT devices"),
    ("wifi7-mlo-throughput", "wifi7", "wifi-bench-mlo-throughput", "",
     "Multi-link operation throughput scaling with 1, 2 and 3 links vs the sum of link bounds"),
    ("wifi7-mlo-latency", "wifi7", "wifi-bench-mlo-latency", "",
     "MLO latency under asymmetric contention vs single-link devices"),
    ("wifi7-emlsr", "wifi7", "wifi-bench-emlsr", "",
     "EMLSR single-radio client vs single-link and dual-radio MLD"),
    ("wifi7-tid-to-link", "wifi7", "wifi-bench-tid-to-link", "",
     "TID-to-link mapping: voice latency isolation from best-effort load"),
    ("wifi7-dynamic-bw", "wifi7", "wifi-bench-dynamic-bw", "",
     "Dynamic bandwidth operation under partial-band interference vs the puncturing bound"),
    # ---- Wi-Fi 8 (802.11bn UHR) ----
    ("wifi8-uhr-kpi", "wifi8", "wifi-bench-uhr-kpi", "",
     "UHR KPI baselines (5th-percentile throughput, P95 latency) in a dense co-channel deployment"),
    ("wifi8-multi-ap", "wifi8", "wifi-bench-multi-ap", "",
     "Multi-AP coordination emulation: uncoordinated vs Co-TDMA vs Co-SR"),
    ("wifi8-roaming", "wifi8", "wifi-bench-roaming", "",
     "BSS transition loss and interruption (mobility objective baseline)"),
    ("wifi8-mlo-reliability", "wifi8", "wifi-bench-mlo-reliability", "",
     "Multi-link reliability under hidden interference (deadline miss ratio)"),
]


def find_repo_root(script_path):
    """Return the ns-3 root directory (three levels above this script)."""
    return os.path.abspath(os.path.join(os.path.dirname(script_path), "..", "..", ".."))


def run_benchmark(entry, args, repo_root, output_dir):
    """Run one benchmark and return a result dictionary."""
    name, generation, example, bench_args, description = entry
    quick = "--quick" if args.profile == "quick" else ""
    program_args = " ".join(x for x in [f"--outputDir={output_dir}", bench_args, quick, args.extra_args] if x)
    cmd = [args.ns3, "run", "--no-build", f"{example} {program_args}"]
    log_dir = os.path.join(output_dir, "logs")
    os.makedirs(log_dir, exist_ok=True)
    log_path = os.path.join(log_dir, f"{name}.log")
    result = {
        "name": name,
        "generation": generation,
        "example": example,
        "args": program_args,
        "description": description,
        "command": " ".join(shlex.quote(c) for c in cmd),
        "log": log_path,
    }
    if args.dry_run:
        print(result["command"])
        result["status"] = "dry-run"
        return result
    json_path = os.path.join(output_dir, f"{name}.json")
    if args.summarize_only:
        class Done:  # stands in for a completed process
            returncode = 0 if os.path.exists(json_path) else 1
        proc = Done()
        result["duration_s"] = ""
    else:
        start = time.time()
        with open(log_path, "w", encoding="utf-8") as log:
            proc = subprocess.run(cmd, cwd=repo_root, stdout=log, stderr=subprocess.STDOUT, check=False)
        result["duration_s"] = round(time.time() - start, 1)
    result["exit_code"] = proc.returncode
    rows = []
    if os.path.exists(json_path):
        with open(json_path, encoding="utf-8") as f:
            rows = json.load(f).get("rows", [])
    result["rows"] = rows
    verdicts = [r.get("verdict", "") for r in rows]
    result["pass"] = verdicts.count("PASS")
    result["fail"] = verdicts.count("FAIL")
    result["info"] = verdicts.count("INFO") + verdicts.count("")
    if proc.returncode != 0 and result["fail"] == 0:
        result["status"] = "ERROR"
    elif result["fail"] > 0:
        result["status"] = "FAIL"
    else:
        result["status"] = "PASS"
    print(f"[{result['status']}] {name}: {result['pass']} pass, {result['fail']} fail, "
          f"{result['info']} info ({result['duration_s']} s)")
    return result


def row_label(row):
    """Build a short label for a row from its non-numeric leading cells."""
    parts = []
    for key, value in row.items():
        if key in ("measured", "reference", "deviation_pct", "verdict", "ref_source"):
            continue
        if isinstance(value, str):
            parts.append(f"{key}={value}")
        else:
            parts.append(f"{key}={value}")
        if len(parts) >= 4:
            break
    return ", ".join(parts)


def write_summary(results, output_dir, args):
    """Write summary.md and summary.json."""
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines = [f"# wifi-benchmarks summary ({args.profile} profile, {now})", ""]
    total_fail = sum(1 for r in results if r["status"] in ("FAIL", "ERROR"))
    lines.append(f"Benchmarks: {len(results)}, with failures or errors: {total_fail}")
    lines.append("")
    lines.append("| Benchmark | Generation | Status | PASS | FAIL | INFO | Duration (s) |")
    lines.append("|---|---|---|---|---|---|---|")
    for r in results:
        lines.append(f"| {r['name']} | {r['generation']} | {r['status']} | {r.get('pass', 0)} | "
                     f"{r.get('fail', 0)} | {r.get('info', 0)} | {r.get('duration_s', '')} |")
    lines.append("")
    for r in results:
        lines.append(f"## {r['name']}")
        lines.append("")
        lines.append(r["description"])
        lines.append("")
        lines.append(f"Command: `{r['command']}`")
        lines.append("")
        rows = r.get("rows", [])
        if not rows:
            lines.append("No result rows (see the log).")
            lines.append("")
            continue
        lines.append("| Row | Measured | Reference | Deviation (%) | Verdict | Source |")
        lines.append("|---|---|---|---|---|---|")
        for row in rows:
            lines.append(f"| {row_label(row)} | {row.get('measured', '')} | {row.get('reference', '')} | "
                         f"{row.get('deviation_pct', '')} | {row.get('verdict', '')} | {row.get('ref_source', '')} |")
        lines.append("")
    with open(os.path.join(output_dir, "summary.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    with open(os.path.join(output_dir, "summary.json"), "w", encoding="utf-8") as f:
        json.dump({"profile": args.profile, "date": now, "results": results}, f, indent=2)
    return total_fail


def write_svg(result, output_dir):
    """Write a bar chart of measured vs reference values for the rows of a benchmark."""
    rows = [r for r in result.get("rows", [])
            if isinstance(r.get("measured"), (int, float)) and isinstance(r.get("reference"), (int, float))]
    if not rows:
        return
    chart_dir = os.path.join(output_dir, "charts")
    os.makedirs(chart_dir, exist_ok=True)
    width, height, margin_l, margin_b = 1000, 420, 70, 150
    plot_w, plot_h = width - margin_l - 20, height - margin_b - 30
    vmax = max(max(abs(r["measured"]), abs(r["reference"])) for r in rows) or 1.0
    group_w = plot_w / len(rows)
    bar_w = max(2.0, group_w * 0.35)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
           f'font-family="sans-serif" font-size="11">',
           f'<text x="{margin_l}" y="18" font-size="14">{result["name"]}: measured (blue) vs reference (grey)</text>',
           f'<line x1="{margin_l}" y1="{30 + plot_h}" x2="{margin_l + plot_w}" y2="{30 + plot_h}" stroke="#444"/>',
           f'<line x1="{margin_l}" y1="30" x2="{margin_l}" y2="{30 + plot_h}" stroke="#444"/>']
    for i in range(5):
        v = vmax * i / 4
        y = 30 + plot_h - plot_h * i / 4
        svg.append(f'<text x="{margin_l - 5}" y="{y + 4}" text-anchor="end">{v:.3g}</text>')
        svg.append(f'<line x1="{margin_l}" y1="{y}" x2="{margin_l + plot_w}" y2="{y}" stroke="#ddd"/>')
    for i, r in enumerate(rows):
        x0 = margin_l + i * group_w + group_w * 0.15
        for j, (key, color) in enumerate((("measured", "#3b6fb6"), ("reference", "#999"))):
            h = plot_h * abs(r[key]) / vmax
            svg.append(f'<rect x="{x0 + j * bar_w}" y="{30 + plot_h - h}" width="{bar_w}" height="{h}" fill="{color}"/>')
        label = row_label(r).replace("&", "&amp;").replace("<", "&lt;")[:40]
        verdict = r.get("verdict", "")
        svg.append(f'<text transform="translate({x0 + bar_w},{35 + plot_h}) rotate(60)" font-size="9">'
                   f'{label} [{verdict}]</text>')
    svg.append("</svg>")
    with open(os.path.join(chart_dir, f"{result['name']}.svg"), "w", encoding="utf-8") as f:
        f.write("\n".join(svg))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--profile", choices=("quick", "full"), default="full",
                        help="quick: short runs used by the regression tests; full: complete sweeps")
    parser.add_argument("--generation", action="append", choices=("wifi6", "wifi7", "wifi8", "all"),
                        help="generation(s) to run (default: all)")
    parser.add_argument("--only", default="", help="regular expression selecting benchmark names")
    parser.add_argument("--output-dir", default="wifi-benchmarks-results",
                        help="directory for the CSV/JSON/log files (relative to the ns-3 root)")
    parser.add_argument("--jobs", type=int, default=1, help="number of benchmarks run in parallel")
    parser.add_argument("--ns3", default=None, help="path to the ns3 script (default: <root>/ns3)")
    parser.add_argument("--extra-args", default="", help="arguments appended to every benchmark")
    parser.add_argument("--dry-run", action="store_true", help="print the commands only")
    parser.add_argument("--skip-build", action="store_true", help="do not run './ns3 build' first")
    parser.add_argument("--summarize-only", action="store_true",
                        help="do not run anything; rebuild the summary from the existing JSON files")
    parser.add_argument("--svg", action="store_true", help="write SVG charts of measured vs reference")
    parser.add_argument("--list", action="store_true", help="list the benchmarks and exit")
    args = parser.parse_args()

    repo_root = find_repo_root(__file__)
    if args.ns3 is None:
        args.ns3 = os.path.join(repo_root, "ns3")
    generations = set(args.generation or ["all"])
    selected = [b for b in BENCHMARKS
                if ("all" in generations or b[1] in generations) and re.search(args.only, b[0])]
    if args.list:
        for name, generation, example, bench_args, description in selected:
            print(f"{name:24s} {generation:6s} {example} {bench_args}\n{'':32s}{description}")
        return 0
    if not selected:
        print("No benchmark selected", file=sys.stderr)
        return 1
    output_dir = args.output_dir
    if not os.path.isabs(output_dir):
        output_dir = os.path.join(repo_root, output_dir)
    os.makedirs(output_dir, exist_ok=True)

    if not args.dry_run and not args.skip_build and not args.summarize_only:
        # build once so that the parallel runs do not trigger concurrent builds
        subprocess.run([args.ns3, "build"], cwd=repo_root, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = [pool.submit(run_benchmark, b, args, repo_root, output_dir) for b in selected]
        for fut in futures:
            results.append(fut.result())
    if args.dry_run:
        return 0
    total_fail = write_summary(results, output_dir, args)
    if args.svg:
        for r in results:
            write_svg(r, output_dir)
    print(f"\nSummary written to {os.path.join(output_dir, 'summary.md')}; "
          f"{len(results)} benchmarks, {total_fail} with failures or errors")
    return 1 if total_fail else 0


if __name__ == "__main__":
    sys.exit(main())
