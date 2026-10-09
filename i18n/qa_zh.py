#!/usr/bin/env python3
# ============================================================
# 功能：汉化校对辅助 —— 自动筛查 i18n/zh_CN/*.ts 的可疑译文并导出审核清单。
# 作者：acelan
# 新建时间：2026-10-09
# 修改时间：2026-10-09
# ============================================================
import argparse
import csv
import glob
import html
import re
import sys

CTX_RE = re.compile(r'<context>([\s\S]*?)</context>')
NAME_RE = re.compile(r'<name>(.*?)</name>')
MSG_RE = re.compile(r'<message>[\s\S]*?</message>')
SRC_RE = re.compile(r'<source>([\s\S]*?)</source>')
TR_RE = re.compile(r'<translation(?:\s+[^>]*)?>([\s\S]*?)</translation>')
PH_RE = re.compile(r'%(?:\d+|L\d+|n|N)')
TAG_RE = re.compile(r'<[^>]+>')
ACCEL_RE = re.compile(r'&([A-Za-z])')
CJK_RE = re.compile(r'[\u4e00-\u9fff]')
LATIN_RE = re.compile(r'[A-Za-z]{2,}')

SUSPECT = {
    "罪恶": "sin 误译为「罪恶」，应为「正弦」",
    "推进器": "protractor 误译为「推进器」，应为「量角器」",
    "线性三角": "sine 误译为「线性」，应为「正弦」",
    "重置镜头": "camera 误译为「镜头」，应为「相机」",
    "身体": "body 误译（HTML 场景）",
    "清晰排序": "Clear sorting 误译，应为「清除排序」",
}


def flags_for(src, tr):
    out = []
    if src.strip() == tr.strip():
        out.append("SAME_AS_SOURCE")
    if sorted(PH_RE.findall(src)) != sorted(PH_RE.findall(tr)):
        out.append("PLACEHOLDER")
    s_tags = sorted(TAG_RE.findall(src))
    t_tags = sorted(TAG_RE.findall(tr))
    if len(s_tags) != len(t_tags):
        out.append("HTML_TAG")
    for m in ACCEL_RE.finditer(src):
        if ("&" + m.group(1).upper()) not in tr.upper():
            out.append("ACCEL")
            break
    if not CJK_RE.search(tr) and "SAME_AS_SOURCE" not in out:
        out.append("NO_CJK")
    for bad in SUSPECT:
        if bad in tr:
            out.append("SUSPECT:" + bad)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="+")
    ap.add_argument("-o", "--out", default="/tmp/i18n_review.csv")
    args = ap.parse_args()
    files = []
    for p in args.paths:
        files.extend(sorted(glob.glob(p)))

    rows = []
    for f in files:
        s = open(f, encoding="utf-8").read()
        for cm in CTX_RE.finditer(s):
            cb = cm.group(1)
            nm = NAME_RE.search(cb)
            ctx = html.unescape(nm.group(1)) if nm else ""
            for mm in MSG_RE.finditer(cb):
                blk = mm.group(0)
                sm = SRC_RE.search(blk)
                tm = TR_RE.search(blk)
                if not sm or not tm:
                    continue
                src = html.unescape(sm.group(1))
                tr = html.unescape(tm.group(1))
                if not tr.strip():
                    continue
                fl = flags_for(src, tr)
                if fl:
                    rows.append((f.split("/")[-1], ctx, ",".join(fl), src.replace("\n", "\\n"), tr.replace("\n", "\\n")))

    rows.sort(key=lambda r: r[2])
    with open(args.out, "w", newline="", encoding="utf-8-sig") as fh:
        w = csv.writer(fh)
        w.writerow(["file", "context", "flags", "source", "translation"])
        w.writerows(rows)

    from collections import Counter
    c = Counter()
    for r in rows:
        for fl in r[2].split(","):
            c[fl.split(":")[0]] += 1
    print("review rows:", len(rows), "->", args.out)
    for k, v in c.most_common():
        print("  %-14s %d" % (k, v))


if __name__ == "__main__":
    main()
