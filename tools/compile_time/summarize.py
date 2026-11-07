#!/usr/bin/env python3
"""The CMake instrumentation callback that summarizes where the compile time of a build went.

CMake calls it after `cmake --build` when the project is configured with -DCM_COMPILE_TIME_TRACE=ON, and appends the
path of a v1 index file. The script reads the snippets of that build and the clang -ftime-trace files CMake copied for
them, and writes compile-time-summary.txt into the build directory. It never changes anything else.
"""

import collections
import json
import pathlib
import re
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
PHASES = (
    "Total Frontend",
    "Total InstantiateFunction",
    "Total InstantiateClass",
    "Total CheckConstraintSatisfaction",
    "Total WriteAST",
    "Total Source",
    "Total Backend",
    "Total Module Load",
)
LIBRARIES = re.compile(
    r"(boost/asio|boost/|stdexec|exec/|spdlog|simdjson|slint|catch2|proto/|comms/|libassert|app-window\.h|mp-units)"
)


def outermost(spans):
    """Yields only the spans that no earlier span contains, so that nested events don't count twice."""
    end = -1
    for span in sorted(spans, key=lambda s: (s[0], -s[1])):
        if span[0] >= end:
            end = span[0] + span[1]
            yield span


def source_spans(events):
    """Pairs the begin and end events of included files into (start, duration, file)."""
    stack = []
    for event in sorted((e for e in events if e.get("name") == "Source" and e.get("ph") in ("b", "e")),
                        key=lambda e: e["ts"]):
        if event["ph"] == "b":
            stack.append(event)
        elif stack:
            begin = stack.pop()
            yield begin["ts"], event["ts"] - begin["ts"], begin.get("args", {}).get("detail", "?")


def is_first_party(source, build_dir):
    """Dependencies live in the build directory or in a CPM source cache outside the repository.

    The C++ that Slint generates for the UI also sits in the build directory, so it counts with the dependencies.
    """
    if not source:
        return False
    path = pathlib.Path(source).resolve()
    return path.is_relative_to(REPO_ROOT) and not path.is_relative_to(build_dir)


def relative(path):
    try:
        return str(pathlib.Path(path).resolve().relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def summarize(index_file):
    index = json.loads(pathlib.Path(index_file).read_text())
    data_dir = pathlib.Path(index["dataDir"])
    snippets = []
    for name in index.get("snippets", []):
        try:
            snippets.append(json.loads((data_dir / name).read_text()))
        except (OSError, ValueError):
            continue

    build_dir = pathlib.Path(index["buildDir"]).resolve()
    builds = [s for s in snippets if s.get("role") in ("cmakeBuild", "build")]
    # The module dependency scans are compile snippets too, but they only take milliseconds and aren't compiles.
    all_compiles = [s for s in snippets
                    if s.get("role") == "compile" and "clang-scan-deps" not in s.get("command", "")]
    compiles = [s for s in all_compiles if is_first_party(s.get("source", ""), build_dir)]
    lines = []
    wall = sum(s.get("duration", 0) for s in builds) / 1000
    summed = sum(s.get("duration", 0) for s in compiles) / 1000
    dependencies = sum(s.get("duration", 0) for s in all_compiles) / 1000 - summed
    lines.append(f"Build wall time {wall / 60:.1f} min, {len(compiles)} first-party compile commands, "
                 f"summed compile time {summed / 60:.1f} min")
    if len(all_compiles) > len(compiles):
        lines.append(f"Dependencies: {len(all_compiles) - len(compiles)} compile commands, {dependencies / 60:.1f} min")

    # A ctest run leaves its own snippets behind, which would end up in the next build's index.
    memory = [info[key] for s in snippets if s.get("role") in ("compile", "link", "custom", "cmakeBuild", "build")
              for info in [s.get("dynamicSystemInformation") or {}]
              for key in ("beforeHostMemoryUsed", "afterHostMemoryUsed") if key in info]
    if memory:
        lines.append(f"Peak host memory in use {max(memory) / 1024 / 1024:.1f} GiB")

    phases = collections.Counter()
    templates = collections.defaultdict(lambda: [0.0, 0])
    headers = collections.Counter()
    per_unit = []
    per_dir = collections.Counter()
    for snippet in compiles:
        seconds = snippet.get("duration", 0) / 1000
        source = relative(snippet.get("source", "?"))
        per_unit.append((seconds, source))
        per_dir[source.split("/")[0]] += seconds
        trace = snippet.get("traceFile")
        if not trace:
            continue
        try:
            events = json.loads((data_dir / trace).read_text())["traceEvents"]
        except (OSError, ValueError, KeyError):
            continue
        for event in events:
            if event.get("name") in PHASES:
                phases[event["name"]] += event["dur"] / 1e6
        instantiations = [(e["ts"], e["dur"], e.get("args", {}).get("detail", "?")) for e in events
                          if e.get("ph") == "X" and e.get("name") in ("InstantiateFunction", "InstantiateClass")]
        for _, duration, detail in outermost(instantiations):
            # Template arguments make every instantiation unique, so group by the name in front of them.
            key = re.sub(r"<.*", "<…>", detail)
            templates[key][0] += duration / 1e6
            templates[key][1] += 1
        for _, duration, file in outermost(list(source_spans(events))):
            match = LIBRARIES.search(file)
            headers[match.group(1) if match else file.rsplit("/", 1)[-1]] += duration / 1e6

    lines.append("\nSlowest compile commands:")
    lines += [f"  {seconds:6.1f}s  {source}" for seconds, source in sorted(per_unit, reverse=True)[:15]]
    lines.append("\nTime per phase over all units:")
    lines += [f"  {phases[name] / 60:6.1f} min  {name.removeprefix('Total ')}" for name in PHASES]
    lines.append("\nOutermost template instantiations, grouped by name:")
    lines += [f"  {seconds:7.1f}s  {count:6d}x  {name[:110]}"
              for name, (seconds, count) in sorted(templates.items(), key=lambda item: -item[1][0])[:20]]
    lines.append("\nHeader parsing by library:")
    lines += [f"  {seconds / 60:5.2f} min  {name}" for name, seconds in headers.most_common(10)]
    lines.append("\nTime per directory:")
    lines += [f"  {seconds / 60:6.1f} min  {name}" for name, seconds in per_dir.most_common()]
    if index.get("trace"):
        lines.append(f"\nTimeline for Perfetto or chrome://tracing: {data_dir / index['trace']}")

    out = pathlib.Path(index["buildDir"]) / "compile-time-summary.txt"
    out.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    summarize(sys.argv[-1])
