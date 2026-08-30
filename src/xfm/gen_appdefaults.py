#!/usr/bin/env python3
# Minimal, line-oriented #ifdef/#ifndef/#else/#endif resolver for
# src/xfm/lib/Xfm.cpp -- a real C preprocessor (clang -E, GNU cpp
# -traditional-cpp) chokes on this file's literal backslash-escaped
# resource values (e.g. line 60's `...:\ `, where the backslash is an
# X-resource escape, not a C line-continuation) because both tokenize
# backslash-newline as a splice regardless. This script never tokenizes
# as C -- pure line-based #if-stack handling plus two textual
# substitutions (LIBDIR, and the LOG_TRANSLATION/HIST_TRANSLATION
# object/function-like macros) -- so it's immune to that class of bug.
import re
import sys

DEFINES = {
    "XPM", "ENHANCE_BUGFIX", "USE_NEW_WIDGETS", "ENHANCE_3DICONS",
    "ENHANCE_TXT_FIELD", "ENHANCE_POP_ACCEL", "ENHANCE_HISTORY",
    "ENHANCE_USERINFO", "ENHANCE_MENU", "ENHANCE_TRANSLATIONS",
    "ENHANCE_PERMS", "ENHANCE_SCROLL", "ENHANCE_CURSOR",
    "ENHANCE_SELECTION", "ENHANCE_CMAP", "VIEWPORT_HACK",
}
LIBDIR = "/usr/share/xfm"
XFMVERSION = "1.4.3"

with open(sys.argv[1]) as f:
    lines = f.read().split("\n")

out = []
# stack of (branch_taken_this_level, any_branch_taken_yet, parent_active)
stack = []


def active():
    return all(taken for taken, _ in stack)


log_translation = None  # resolved once we hit its #define, used below

for i, line in enumerate(lines):
    stripped = line.strip()
    if stripped.startswith("#ifdef"):
        macro = stripped.split()[1]
        cond = macro in DEFINES
        stack.append([cond, cond])
        continue
    if stripped.startswith("#ifndef"):
        macro = stripped.split()[1]
        cond = macro not in DEFINES
        stack.append([cond, cond])
        continue
    if stripped.startswith("#else"):
        taken, any_taken = stack[-1]
        stack[-1] = [not any_taken, True]
        continue
    if stripped.startswith("#endif"):
        stack.pop()
        continue
    if not active():
        continue
    if stripped.startswith("#define LOG_TRANSLATION"):
        rest = stripped[len("#define LOG_TRANSLATION"):].strip()
        log_translation = rest
        continue
    if stripped.startswith("#define HIST_TRANSLATION"):
        # captured structurally below via regex at use-sites instead
        continue
    if stripped.startswith("#include"):
        # only src/FmVersion.h, used solely for XFMVERSION -- already
        # substituted directly below.
        continue
    out.append(line)

text = "\n".join(out)

# LIBDIR word-boundary substitution (matches cpp's object-like macro
# expansion of a bare identifier token).
text = re.sub(r"\bLIBDIR\b", LIBDIR, text)
text = re.sub(r"\bXFMVERSION\b", '"' + XFMVERSION + '"', text)

# LOG_TRANSLATION: object-like macro, either the ENHANCE_LOG-enabled
# translation-table line or empty (ENHANCE_LOG is off upstream by
# default and stays off here -- no font/bitmap-rendering log window
# beyond what everything else in this project already lacks).
text = re.sub(r"^LOG_TRANSLATION \\$", (log_translation or "").rstrip("\\").rstrip(), text, flags=re.M)
if not log_translation:
    # drop the now-empty line entirely (upstream's own #else branch
    # defines it to nothing, i.e. the line vanishes)
    text = re.sub(r"\n\s*\n", "\n", text)
    text = text.replace("LOG_TRANSLATION \\\n", "")

# HIST_TRANSLATION(a,b): function-like macro, ENHANCE_HISTORY is on, so
# expand to its real body with a,b substituted (only two call-site
# argument shapes appear in this file: (FocusSet(), ) and (,)).
HIST_BODY = '<Btn3Down>:{a} FmUpdateHistory(fm_history)MenuPopup(fm_history) {b}\\n'


def hist_sub(m):
    a, b = m.group(1).strip(), m.group(2).strip()
    return HIST_BODY.format(a=a, b=b) + " \\" if m.group(0).endswith("\\") else HIST_BODY.format(a=a, b=b)


ARG = r"(?:[^(),]|\([^()]*\))*"
text = re.sub(r"HIST_TRANSLATION\((" + ARG + r"),(" + ARG + r")\)\s*(\\?)",
              lambda m: HIST_BODY.format(a=m.group(1).strip(), b=m.group(2).strip()) + (" \\" if m.group(3) else ""),
              text)

sys.stdout.write(text)
