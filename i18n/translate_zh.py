#!/usr/bin/env python3
# ============================================================
# 功能：批量机器翻译 i18n/zh_CN/*.ts 中未翻译的条目（Google MT 端点），
#       保护 %1 占位符、<html> 标签、字面 \n 与 & 加速键。
# 作者：acelan
# 新建时间：2026-10-09
# 修改时间：2026-10-09
# ============================================================
import argparse
import glob
import html
import json
import os
import re
import sys
import threading
import time
import urllib.parse
import urllib.request

try:
    import argostranslate.translate as _argos
except Exception:
    _argos = None

ENDPOINT = "https://translate.googleapis.com/translate_a/single"
BATCH = 20
WORKERS = 1
CACHE_FILE = "/tmp/zh_MT_cache.json"
_lock = threading.Lock()
_done = 0
_total = 0

GLOSSARY = {
    "OK": "确定", "Apply": "应用", "Reset": "重置", "Close": "关闭", "Cancel": "取消",
    "Delete": "删除", "Remove": "移除", "Add": "添加", "Open": "打开", "Save": "保存",
    "Edit": "编辑", "View": "视图", "Help": "帮助", "Filter": "过滤器", "Filters": "过滤器",
    "Clear": "清除", "Generate": "生成", "Properties": "属性", "Information": "信息",
    "Display": "显示", "Name": "名称", "Value": "值", "Type": "类型", "Color": "颜色",
    "Size": "大小", "Width": "宽度", "Height": "高度", "Data": "数据", "File": "文件",
    "Image": "图像", "Table": "表格", "Selection": "选择", "Range": "范围",
    "Custom": "自定义", "Auto": "自动", "None": "无", "Default": "默认", "All": "全部",
    "Enabled": "启用", "Disabled": "禁用", "Show": "显示", "Hide": "隐藏", "Axis": "轴",
    "Point": "点", "Yes": "是", "No": "否", "Clear sorting": "清除排序",
    "Sample series:": "采样序列：", "Sample": "采样", "Abort": "中止",
    "Protractor": "量角器", "sin": "正弦", "Sine": "正弦", "Reset Camera": "重置相机",
    "Reposition protractor to view": "重新放置量角器到视图",
}
TERM_FIX = {"推进器": "量角器", "罪恶": "正弦", "线性三角": "正弦三角函数"}


def _http(url, data=None, headers=None):
    req = urllib.request.Request(url, data=data, headers=headers or {"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=25) as r:
        return r.read().decode("utf-8")


def _fix(t):
    t = re.sub(r'%\s+(\d)', r'%\1', t)
    t = re.sub(r'\\\s+n', r'\\n', t)
    return t.replace('＆', '&')


def my_memory(text):
    for a in range(3):
        try:
            j = json.loads(_http("https://api.mymemory.translated.net/get?" + urllib.parse.urlencode(
                {"q": text, "langpair": "en|zh-CN"})))
            if j.get("quotaFinished"):
                return None
            t = j.get("responseData", {}).get("translatedText")
            return _fix(t) if t else None
        except Exception:
            time.sleep(0.8 * (a + 1))
    return None


def google(text):
    for a in range(2):
        try:
            j = json.loads(_http("https://translate.googleapis.com/translate_a/single?" + urllib.parse.urlencode(
                {"client": "gtx", "sl": "en", "tl": "zh-CN", "dt": "t", "q": text})))
            return _fix("".join(s[0] for s in j[0]))
        except Exception:
            time.sleep(1.0 * (a + 1))
    return None


def youdao(text):
    for a in range(2):
        try:
            data = urllib.parse.urlencode({"q": text, "from": "en", "to": "zh-CHS"}).encode()
            j = json.loads(_http("https://aidemo.youdao.com/trans", data,
                {"User-Agent": "Mozilla/5.0", "Content-Type": "application/x-www-form-urlencoded"}))
            t = j.get("translation")
            if t:
                v = t[0]
                v = v.replace('< ', '<').replace(' >', '>').replace('</ ', '</').replace(' /', '/')
                return _fix(v)
        except Exception:
            time.sleep(1.0 * (a + 1))
    return None


def mt(text):
    for fn in (argos, my_memory, google, youdao):
        r = fn(text)
        if r is not None and r.strip():
            return r
    return None


def argos(text):
    if _argos is None:
        return None
    try:
        r = _argos.translate(text, "en", "zh")
        return _fix(r) if r else None
    except Exception:
        return None


def preprocess(src):
    accel = None
    text = src
    m = re.search(r'&([A-Za-z])', src)
    if m and src.count('&') == 1 and len(src) <= 60:
        accel = m.group(1).upper()
        text = src[:m.start()] + src[m.end():]
    text = text.replace('\n', ' ')
    return text, accel


def postprocess(out, accel):
    out = _fix(out).strip()
    for a, b in TERM_FIX.items():
        out = out.replace(a, b)
    if accel and out:
        out = "%s(&%s)" % (out, accel)
    return out


def mt_src(text):
    if re.search(r'<[A-Za-z][^>]*>', text):
        parts = re.split(r'(<[^>]+>)', text)
        out = []
        for p in parts:
            if p.startswith('<') and p.endswith('>'):
                out.append(p)
            elif re.search(r'[A-Za-z]{2}', p):
                r = mt(p)
                out.append(r if r else p)
            else:
                out.append(p)
        return "".join(out)
    return mt(text)


def translate_unique(sources):
    global _done, _total
    uniq = [s for s in dict.fromkeys(sources) if re.search(r'[A-Za-z]', s.replace('\n', ' '))]
    cache = {}
    if os.path.exists(CACHE_FILE):
        try:
            cache = json.load(open(CACHE_FILE, encoding="utf-8"))
        except Exception:
            cache = {}
    todo = [s for s in uniq if s not in cache]
    _total = len(uniq)
    _done = len(uniq) - len(todo)
    print("unique=%d cached=%d to_translate=%d" % (len(uniq), _done, len(todo)), flush=True)

    def work(s):
        global _done
        pre, accel = preprocess(s)
        o = mt_src(pre)
        val = postprocess(o, accel) if o is not None else None
        base = GLOSSARY.get(pre.strip())
        if base:
            val = base + (("(&%s)" % accel) if accel else "")
        with _lock:
            if val is not None:
                cache[s] = val
            _done += 1
            if _done % 200 == 0:
                json.dump(cache, open(CACHE_FILE, "w", encoding="utf-8"))
                sys.stderr.write("\r  %d/%d" % (_done, _total))
                sys.stderr.flush()

    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(max_workers=WORKERS) as ex:
        list(ex.map(work, todo))
    json.dump(cache, open(CACHE_FILE, "w", encoding="utf-8"))
    sys.stderr.write("\n")
    return {s: cache[s] for s in uniq if s in cache}


MSG_RE = re.compile(r'<message>[\s\S]*?</message>')
SRC_RE = re.compile(r'<source>([\s\S]*?)</source>')
TR_RE = re.compile(r'(<translation)(?:\s+[^>]*)?>([\s\S]*?)</translation>')
HTML_RE = re.compile(r'<[A-Za-z][^>]*>')


def collect(path, force_html=False):
    s = open(path, encoding="utf-8").read()
    todo = []
    for m in MSG_RE.finditer(s):
        blk = m.group(0)
        sm = SRC_RE.search(blk)
        tm = TR_RE.search(blk)
        if not sm or not tm:
            continue
        if 'type="vanished"' in tm.group(0):
            continue
        src = html.unescape(sm.group(1))
        if tm.group(2).strip() and not (force_html and HTML_RE.search(src)):
            continue
        todo.append(src)
    return s, todo


def apply(path, s, mapping, force_html=False):
    def repl(m):
        blk = m.group(0)
        sm = SRC_RE.search(blk)
        tm = TR_RE.search(blk)
        if not sm or not tm or 'type="vanished"' in tm.group(0):
            return blk
        src = html.unescape(sm.group(1))
        if tm.group(2).strip() and not (force_html and HTML_RE.search(src)):
            return blk
        val = mapping.get(src)
        if not val:
            return blk
        esc = html.escape(val, quote=False)
        newtr = "<translation>%s</translation>" % esc
        return blk[:tm.start()] + newtr + blk[tm.end():]
    return MSG_RE.sub(repl, s)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--force-html", action="store_true")
    args = ap.parse_args()

    files = []
    for p in args.paths:
        files.extend(sorted(glob.glob(p)))
    collected = {}
    all_sources = []
    for f in files:
        s, todo = collect(f, args.force_html)
        collected[f] = (s, todo)
        all_sources.extend(todo)
    print("files: %d | entries to translate: %d" % (len(files), len(all_sources)))
    if args.limit:
        all_sources = all_sources[:args.limit]
    mapping = translate_unique(all_sources)
    print("translated unique: %d" % len(mapping))

    for f in files:
        s, todo = collected[f]
        s = apply(f, s, mapping, args.force_html)
        for a, b in TERM_FIX.items():
            s = s.replace(a, b)
        open(f, "w", encoding="utf-8").write(s)
        print("  wrote", f)


if __name__ == "__main__":
    main()
