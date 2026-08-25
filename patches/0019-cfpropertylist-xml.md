# Patch: CFPropertyList (XML plist) in CoreFoundation

**Why.** Phase 25's real `config.defs` (SystemConfiguration/configd) wire
protocol carries `xmlData`/`xmlDataOut` MIG types — literally serialized
XML property list bytes. This project's CoreFoundation had never
implemented `CFPropertyList` at all (explicitly listed as cut in
`CoreFoundation.h`'s own header comment).

**What.** New `userland/CoreFoundation/CFPropertyList.{h,c}`, wired into
the umbrella header and `build.sh`'s object list. The classic pre-CFError
API pair — `CFPropertyListCreateXMLData`/`CFPropertyListCreateFromXMLData`
— rather than the modern `CFErrorRef`-based one, since this project has
no `CFErrorRef` type yet and this is the exact pair real callers of the
old API (which real Apple still ships alongside the new one) would use.
XML only, no binary plist (`bplist00`) — nothing this project needs
produces or expects binary plists.

Written from scratch, not vendored: real CF's XML plist code is
entangled with CFRunLoop-adjacent internal machinery this project doesn't
have, so there's no single clean upstream `.c` file to port the way
`config.defs` itself will be. Supports the property types
`config.defs`/`SCDynamicStore` actually need — `CFDictionary`, `CFArray`,
`CFString`, `CFNumber` (int and float), `CFBoolean`, `CFData` — matching
classic CF's own plist DTD, which never represented `CFDate` (this
vintage's DTD has no `<date>` tag) or `CFNull` (property lists never
allowed it as a value) either.

Base64 (for `<data>` elements) verified correct against known reference
vectors (`"f"`→`"Zg=="`, `"fo"`→`"Zm8="`, `"foo"`→`"Zm9v"`, the standard
RFC 4648 test strings) via a standalone host-side harness before wiring
into CF proper, since this project's own binaries can't run natively on
the build host to test directly — full integration (a real
`CFDictionary` round-tripping through `CreateXMLData`→`CreateFromXMLData`
inside this project's actual CF runtime) is verified live in QEMU as part
of Phase 25's own end-to-end test, not standalone.

`kCFPropertyListXMLFormatVersion1_0` is the one place this deviates from
the `kCFNull`/`kCFBooleanTrue` singleton pattern already established in
`CFNull.c`/`CFBoolean.c` (a `const`-qualified global whose *address* is a
compile-time constant, populated later by an `__attribute__((constructor))`)
— `CFStringCreateWithCString()` isn't a constant expression, so that
exact trick doesn't apply to a `CFStringRef` singleton; it's declared
without the top-level `const` instead and assigned directly in its own
constructor. The pointee is still effectively immutable either way
(`CFStringRef` already points to `const` storage).
