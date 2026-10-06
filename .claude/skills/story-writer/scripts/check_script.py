#!/usr/bin/env python3
"""Mechanical checks on a story script.

Reads a markdown file containing scenes ("### Scene ..." headings), dialogue
lines ("NAME: text") and stage directions ("[...]"). Reports spoken word count,
per-scene counts, and dialogue lines over the length limits.

Usage: check_script.py FILE [--budget MIN-MAX] [--soft 25] [--hard 40]
"""
import argparse
import re
import sys

SCENE_RE = re.compile(r"^#{2,4}\s*Scene\b(.*)", re.IGNORECASE)
LINE_RE = re.compile(r"^\s*\**([A-Z][A-Z0-9 .'\-]*?)\**\s*:\s*(.+)$")


def words(text):
    return len(re.findall(r"[\w'’-]+", text))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("--budget", help="spoken word range, e.g. 2000-3500")
    ap.add_argument("--soft", type=int, default=25, help="average line target")
    ap.add_argument("--hard", type=int, default=40, help="per-line cap")
    args = ap.parse_args()

    scenes = []  # [title, line_count, word_count, speakers]
    long_lines = []
    all_counts = []
    current = None
    with open(args.file, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            m = SCENE_RE.match(raw)
            if m:
                current = [m.group(1).strip(" -—:") or f"line {n}", 0, 0, set()]
                scenes.append(current)
                continue
            if current is None:
                continue  # sheets and cards before the first scene aren't spoken
            m = LINE_RE.match(raw)
            if not m:
                continue
            speaker, text = m.group(1).strip(), m.group(2)
            text = re.sub(r"\[[^\]]*\]", "", text)  # inline directions aren't spoken
            w = words(text)
            current[1] += 1
            current[2] += w
            current[3].add(speaker)
            all_counts.append(w)
            if w > args.hard:
                long_lines.append((n, speaker, w))

    if not scenes:
        print("No '### Scene' headings found; nothing to check.")
        return 1

    total = sum(s[2] for s in scenes)
    speakers = set().union(*(s[3] for s in scenes))
    avg = total / len(all_counts) if all_counts else 0
    print(f"Scenes: {len(scenes)}   Dialogue lines: {len(all_counts)}   Spoken words: {total}")
    print(f"Speaking roles: {len(speakers)} ({', '.join(sorted(speakers))})")
    print(f"Average line: {avg:.1f} words (target under {args.soft})")
    print()
    for i, (title, lines, w, sp) in enumerate(scenes, 1):
        print(f"  {i:>2}. {title[:50]:<50} {lines:>3} lines {w:>5} words")

    problems = 0
    if avg >= args.soft:
        print(f"\nWARN average line length {avg:.1f} >= {args.soft}")
        problems += 1
    for n, speaker, w in long_lines:
        print(f"WARN line {n}: {speaker} has {w} words (cap {args.hard}; allowed only for a single joke or confession)")
        problems += 1
    if args.budget:
        lo, hi = (int(x) for x in args.budget.split("-"))
        if not lo <= total <= hi:
            print(f"WARN spoken words {total} outside budget {lo}-{hi}")
            problems += 1
    print("\nOK" if not problems else f"\n{problems} warning(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
