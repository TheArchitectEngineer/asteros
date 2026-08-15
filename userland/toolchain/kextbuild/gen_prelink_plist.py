#!/usr/bin/env python3
"""Generates the __PRELINK_INFO XML plist KLDBootstrap::readPrelinkedExtensions
unserializes at boot -- Phase 23 (see TODO.md). Keys used here were
ground-truthed against OSKext::initWithPrelinkedInfoDict()/
setInfoDictionaryAndPath()/resolveDependencies() (OSKext.cpp), not guessed
from a generic prelink-format reference -- no kextcache/kmutil exists in
this tree to generate one for us.

Includes four minimal "codeless" placeholder dicts for the KPI bundle IDs
HelloKext depends on (com.apple.kpi.iokit/libkern/mach/bsd): confirmed via
that same trace that loadKernelComponentKexts() does NOT auto-register
these sub-identifiers (only the kernel itself, as "com.apple.kernel", is
auto-registered) -- resolveDependencies() hard-fails
("library kext %s not found.") without a real OSKext registered under
each name a dependent kext lists in OSBundleLibraries. A dict with just
CFBundleIdentifier+CFBundleVersion (no CFBundleExecutable) makes
declaresExecutable() false, so it's treated as a codeless library and
skips the prelinked-executable-presence checks entirely.
"""
import sys

KEXT_PLIST_TMPL = """\t\t<dict>
\t\t\t<key>CFBundleIdentifier</key>
\t\t\t<string>{bundle_id}</string>
\t\t\t<key>CFBundleVersion</key>
\t\t\t<string>{version}</string>
\t\t\t<key>CFBundleExecutable</key>
\t\t\t<string>{executable}</string>
\t\t\t<key>_PrelinkExecutableLoadAddr</key>
\t\t\t<integer size="64">0x{load_addr:x}</integer>
\t\t\t<key>_PrelinkExecutableSize</key>
\t\t\t<integer size="64">{exec_size}</integer>
\t\t\t<key>_PrelinkKmodInfo</key>
\t\t\t<integer size="64">0x{kmod_info_addr:x}</integer>
\t\t\t<key>OSBundleLibraries</key>
\t\t\t<dict>
{libs}
\t\t\t</dict>
\t\t\t<key>IOKitPersonalities</key>
\t\t\t<dict>
{personalities}
\t\t\t</dict>
\t\t</dict>
"""

CODELESS_TMPL = """\t\t<dict>
\t\t\t<key>CFBundleIdentifier</key>
\t\t\t<string>{bundle_id}</string>
\t\t\t<key>CFBundleVersion</key>
\t\t\t<string>{version}</string>
\t\t</dict>
"""

KPI_VERSION = "1.0.0"
KPIS = [
    "com.apple.kpi.iokit",
    "com.apple.kpi.libkern",
    "com.apple.kpi.mach",
    "com.apple.kpi.bsd",
]

PERSONALITY_TMPL = """\t\t\t\t<key>{personality_name}</key>
\t\t\t\t<dict>
\t\t\t\t\t<key>CFBundleIdentifier</key>
\t\t\t\t\t<string>{bundle_id}</string>
\t\t\t\t\t<key>IOClass</key>
\t\t\t\t\t<string>{io_class}</string>
\t\t\t\t\t<key>IOProviderClass</key>
\t\t\t\t\t<string>{provider_class}</string>
\t\t\t\t\t<key>IOMatchCategory</key>
\t\t\t\t\t<string>{io_class}</string>
\t\t\t\t\t<key>IOProbeScore</key>
\t\t\t\t\t<integer size="32">1000</integer>
\t\t\t\t</dict>
"""


def main():
    if len(sys.argv) != 9:
        sys.stderr.write(
            "usage: gen_prelink_plist.py <bundle_id> <version> <executable_name> "
            "<io_class> <load_addr_hex> <exec_size> <kmod_info_addr_hex> <out_plist>\n")
        sys.exit(1)

    (bundle_id, version, executable, io_class, load_addr_hex, exec_size_s,
     kmod_hex, out_path) = sys.argv[1:9]

    load_addr = int(load_addr_hex, 16)
    kmod_addr = int(kmod_hex, 16)
    exec_size = int(exec_size_s)

    libs = "\n".join(
        "\t\t\t\t<key>%s</key>\n\t\t\t\t<string>%s</string>" % (k, KPI_VERSION)
        for k in KPIS
    )
    personalities = PERSONALITY_TMPL.format(
        personality_name=bundle_id, bundle_id=bundle_id, io_class=io_class,
        provider_class="IOResources")

    kext_dict = KEXT_PLIST_TMPL.format(
        bundle_id=bundle_id, version=version, executable=executable,
        load_addr=load_addr, exec_size=exec_size, kmod_info_addr=kmod_addr,
        libs=libs, personalities=personalities)

    codeless = "".join(
        CODELESS_TMPL.format(bundle_id=k, version=KPI_VERSION) for k in KPIS
    )

    plist = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
\t<key>_PrelinkInfoDictionary</key>
\t<array>
{kext_dict}{codeless}\t</array>
</dict>
</plist>
""".format(kext_dict=kext_dict, codeless=codeless)

    with open(out_path, "wb") as f:
        f.write(plist.encode("utf-8"))
    sys.stderr.write("gen_prelink_plist: wrote %s (%d bytes)\n" % (out_path, len(plist)))


if __name__ == "__main__":
    main()
