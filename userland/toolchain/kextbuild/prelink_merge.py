#!/usr/bin/env python3
"""Merges a real kxld-linked kext plus its __PRELINK_INFO plist into a
kernel Mach-O image, producing a genuine bootable prelinked kernel --
Phase 23 of the SystemConfiguration port (see TODO.md).

v3 design -- back to grow-in-place (v1's placement), now that the real
blocker is fixed at its actual root (see OSKext.cpp's new
osdata_prelinked_kext_free()): v2 tried appending at kext_alloc_base to
land inside g_kext_map, on the theory that kext_free()'s "no map entry"
panic was purely an address-range problem. It wasn't -- kxld/OSKext's own
prelinked-kext path (OSKext::initWithPrelinkedInfoDict(), OSKext.cpp
~line 1567 on) only ever calls kext_alloc()/vm_map_enter() against
g_kext_map when a kPrelinkExecutableSourceKey is present in the info dict
(i.e. "load address differs from source address, copy it"); this project's
gen_prelink_plist.py never emits that key, so the prelinked executable's
memory is *never* entered into g_kext_map at all, no matter where it's
placed -- kext_free()'s mach_vm_deallocate(g_kext_map, ...) was always
going to panic "no map entry" against it, in v1's placement AND in v2's.
v2's kext_alloc_base placement additionally turned out to be physically
unreachable at UEFI boot time in QEMU/OVMF regardless of RAM size (see
TODO.md), an entirely separate problem it introduced for no benefit.

The real fix is on the kernel side: prelinked-kext executable memory is
boot-time-static (placed directly by boot/boot.c's UEFI loader, exactly
like the kernel's own segments), never dynamically kext_alloc()'d, and
this project never unloads kexts anyway -- so its OSData dealloc function
should be a no-op instead of routing through kext_free()/g_kext_map. That
patch (osdata_prelinked_kext_free in OSKext.cpp) makes the *address*
irrelevant to correctness again, so this tool goes back to v1's simpler,
already-UEFI-proven placement: grow the existing (currently zero-sized)
__PRELINK_TEXT/__PRELINK_INFO segments in place, right after the kernel's
own last real segment, in the ~1.9MB gap that already exists before the
FAT16 RAMDisk's fixed low-32-bit physical load address (an arbitrary new
address, e.g. kext_alloc_base, risks colliding with the RAMDisk or landing
outside boot.c's simple "phys = vmaddr's low 32 bits" reachable range --
this gap doesn't, and boot already proved it reachable in the original v1
attempt).

Every load command whose fields reference a file offset at or past the
insertion point gets shifted by the inserted size -- enumerated directly
from this kernel's own `otool -l` output (LC_SEGMENT_64 for __CTF /
__LINKEDIT, LC_SYMTAB, LC_DYSYMTAB's indirectsymoff/locreloff, and
LC_FUNCTION_STARTS), not assumed from a generic Mach-O reference. __CTF's
*section* offset (a separate field from its segment's fileoff) must be
shifted too -- v1's very first attempt forgot this and produced a kernel
image `nm` refused to read ("section contents ... overlaps").
"""
import struct
import sys

PAGE = 0x1000


def page_align(n):
    return (n + PAGE - 1) & ~(PAGE - 1)


def read_cstr(b):
    return b.split(b"\x00", 1)[0].decode("ascii")


class MachO:
    def __init__(self, data: bytearray):
        self.data = data
        (self.magic, self.cputype, self.cpusubtype, self.filetype,
         self.ncmds, self.sizeofcmds, self.flags, self.reserved) = \
            struct.unpack_from("<IiiIIIII", data, 0)
        if self.magic != 0xfeedfacf:
            raise ValueError("not a 64-bit Mach-O (magic=0x%x)" % self.magic)
        self.header_size = 32
        self.commands = []
        off = self.header_size
        for _ in range(self.ncmds):
            cmd, cmdsize = struct.unpack_from("<II", data, off)
            self.commands.append((off, cmd, cmdsize))
            off += cmdsize

    def find_segment(self, name):
        LC_SEGMENT_64 = 0x19
        for off, cmd, cmdsize in self.commands:
            if cmd != LC_SEGMENT_64:
                continue
            segname = read_cstr(self.data[off + 8:off + 24])
            if segname == name:
                return off
        raise KeyError(name)

    def find_command(self, wanted_cmd):
        for off, cmd, cmdsize in self.commands:
            if cmd == wanted_cmd:
                return off
        raise KeyError(wanted_cmd)

    def seg_fields(self, off):
        (cmd, cmdsize, segname, vmaddr, vmsize, fileoff, filesize,
         maxprot, initprot, nsects, flags) = struct.unpack_from(
            "<II16sQQQQiiII", self.data, off)
        return dict(off=off, cmdsize=cmdsize, segname=segname, vmaddr=vmaddr,
                    vmsize=vmsize, fileoff=fileoff, filesize=filesize,
                    maxprot=maxprot, initprot=initprot, nsects=nsects, flags=flags)

    def set_seg(self, off, vmaddr, vmsize, fileoff, filesize):
        struct.pack_into("<QQQQ", self.data, off + 24, vmaddr, vmsize, fileoff, filesize)

    def set_prot(self, off, maxprot, initprot):
        struct.pack_into("<ii", self.data, off + 56, maxprot, initprot)

    def set_section(self, seg_off, index, addr, size, offset):
        sect_off = seg_off + 72 + index * 80
        struct.pack_into("<QQI", self.data, sect_off + 32, addr, size, offset)

    def get_section_offset(self, seg_off, index):
        sect_off = seg_off + 72 + index * 80
        return struct.unpack_from("<I", self.data, sect_off + 48)[0]

    def set_section_offset(self, seg_off, index, offset):
        sect_off = seg_off + 72 + index * 80
        struct.pack_into("<I", self.data, sect_off + 48, offset)


def die(msg):
    sys.stderr.write("prelink_merge: %s\n" % msg)
    sys.exit(1)


LC_SYMTAB = 0x2
LC_DYSYMTAB = 0xb
LC_FUNCTION_STARTS = 0x26


def main():
    if len(sys.argv) != 5:
        die("usage: prelink_merge.py <kernel_in> <linked_kext> <plist> <kernel_out>")

    kernel_path, kext_path, plist_path, out_path = sys.argv[1:5]

    with open(kernel_path, "rb") as f:
        data = bytearray(f.read())
    with open(kext_path, "rb") as f:
        kext_bytes = f.read()
    with open(plist_path, "rb") as f:
        plist_bytes = f.read()

    mo = MachO(data)

    pt_off = mo.find_segment("__PRELINK_TEXT")
    pi_off = mo.find_segment("__PRELINK_INFO")
    ctf_off = mo.find_segment("__CTF")
    linkedit_off = mo.find_segment("__LINKEDIT")
    pt = mo.seg_fields(pt_off)
    pi = mo.seg_fields(pi_off)
    ctf = mo.seg_fields(ctf_off)
    linkedit = mo.seg_fields(linkedit_off)

    if pt["vmsize"] != 0 or pi["vmsize"] != 0:
        die("__PRELINK_TEXT/__PRELINK_INFO are already non-empty -- this kernel "
            "has already been prelinked once; this tool only supports a single merge pass")

    insert_point = pt["fileoff"]
    if pi["fileoff"] != insert_point or ctf["fileoff"] != insert_point:
        die("unexpected layout: __PRELINK_TEXT/__PRELINK_INFO/__CTF don't all "
            "start at the same fileoff (0x%x) -- this tool's shift logic assumes "
            "they do, re-check with otool -l before proceeding" % insert_point)

    insert_vmaddr = pt["vmaddr"]

    kext_padded = page_align(len(kext_bytes))
    plist_padded = page_align(len(plist_bytes))
    insert_size = kext_padded + plist_padded

    # __PRELINK_TEXT and __PRELINK_INFO grow from zero to real content,
    # placed back-to-back starting exactly where they already sit.
    new_pt_vmaddr = insert_vmaddr
    new_pt_fileoff = insert_point
    new_pi_vmaddr = new_pt_vmaddr + kext_padded
    new_pi_fileoff = new_pt_fileoff + kext_padded

    mo.set_seg(pt_off, new_pt_vmaddr, kext_padded, new_pt_fileoff, kext_padded)
    mo.set_section(pt_off, 0, new_pt_vmaddr, len(kext_bytes), new_pt_fileoff)
    # __PRELINK_TEXT's own maxprot/initprot were 0x3 (RW, no execute) --
    # correct for the placeholder zero-sized segment it used to be, but
    # this segment now houses a real kext's real __TEXT (maxprot 0x5,
    # R+X). Caught live: OSKext::setVMAttributes()'s vm_map_protect() call
    # for the kext's own __TEXT segment failed with KERN_PROTECTION_FAILURE
    # (0x2) -- a VM map entry's maxprot is a ceiling that can only be
    # narrowed later, never raised, and whatever early VM bootstrap set up
    # this memory's initial map entry read it from this exact field on the
    # *kernel's* __PRELINK_TEXT load command, not from anything kext-side.
    # Set it to VM_PROT_ALL (RWX) here -- a generous ceiling for the
    # container segment -- letting each individual kext's own segments
    # (protected later, narrower, via OSKext_protect) actually stay within
    # it, matching real Apple's prelinked kernels where __PRELINK_TEXT is
    # part of the kernel's own initial image and already carries this.
    mo.set_prot(pt_off, 0x7, 0x3)

    mo.set_seg(pi_off, new_pi_vmaddr, plist_padded, new_pi_fileoff, plist_padded)
    mo.set_section(pi_off, 0, new_pi_vmaddr, len(plist_bytes), new_pi_fileoff)

    # Everything that used to start at insert_point (__CTF's real file
    # content, currently coincident with __PRELINK_TEXT/INFO's empty slot)
    # and everything after it in the file shifts forward by insert_size.
    new_ctf_fileoff = ctf["fileoff"] + insert_size
    mo.set_seg(ctf_off, ctf["vmaddr"], ctf["vmsize"], new_ctf_fileoff, ctf["filesize"])
    old_ctf_sect_off = mo.get_section_offset(ctf_off, 0)
    mo.set_section_offset(ctf_off, 0, old_ctf_sect_off + insert_size)

    # __LINKEDIT's *vmaddr*, not just its fileoff, must move too: before
    # this merge it shared insert_vmaddr with the still-empty
    # __PRELINK_TEXT/__PRELINK_INFO/__CTF (Apple's convention of collapsing
    # zero-vmsize segments onto one address) -- now that __PRELINK_TEXT and
    # __PRELINK_INFO have real, nonzero vmsize occupying that address range
    # for real, leaving __LINKEDIT's vmaddr untouched makes it claim the
    # exact same physical range boot.c's UEFI loader just allocated for
    # __PRELINK_TEXT. Caught live: boot.c's per-segment AllocatePages(
    # AllocateAddress, ...) call for __LINKEDIT itself failed (not
    # __PRELINK_TEXT/INFO, which loaded fine first) -- EFI_NOT_FOUND is
    # UEFI's generic "no page description satisfies this exact request"
    # here really meaning "these physical pages are already allocated by
    # the __PRELINK_TEXT segment two segments back in this same loop".
    new_linkedit_vmaddr = linkedit["vmaddr"] + insert_size
    new_linkedit_fileoff = linkedit["fileoff"] + insert_size
    mo.set_seg(linkedit_off, new_linkedit_vmaddr, linkedit["vmsize"],
               new_linkedit_fileoff, linkedit["filesize"])

    symtab_off = mo.find_command(LC_SYMTAB)
    _, _, symoff, nsyms, stroff, strsize = struct.unpack_from("<IIIIII", data, symtab_off)
    struct.pack_into("<IIIIII", data, symtab_off,
                      LC_SYMTAB, 24, symoff + insert_size, nsyms, stroff + insert_size, strsize)

    dysymtab_off = mo.find_command(LC_DYSYMTAB)
    dys = list(struct.unpack_from("<" + "I" * 20, data, dysymtab_off))
    # struct dysymtab_command field order (index): cmd,cmdsize,ilocalsym,
    # nlocalsym,iextdefsym,nextdefsym,iundefsym,nundefsym,tocoff,ntoc,
    # modtaboff,nmodtab,extrefsymoff,nextrefsyms,indirectsymoff,
    # nindirectsyms,extreloff,nextrel,locreloff,nlocrel
    for idx in (8, 10, 12, 14, 16, 18):  # the *off fields (skip counts)
        if dys[idx] != 0:
            dys[idx] += insert_size
    struct.pack_into("<" + "I" * 20, data, dysymtab_off, *dys)

    try:
        fs_off = mo.find_command(LC_FUNCTION_STARTS)
        _, cmdsize, dataoff, datasize = struct.unpack_from("<IIII", data, fs_off)
        struct.pack_into("<IIII", data, fs_off, LC_FUNCTION_STARTS, cmdsize,
                          dataoff + insert_size, datasize)
    except KeyError:
        pass

    # Physically splice the new content in at insert_point, pushing
    # everything from there on (starting with __CTF's real bytes) forward.
    appended = bytearray(kext_padded + plist_padded)
    appended[0:len(kext_bytes)] = kext_bytes
    appended[kext_padded:kext_padded + len(plist_bytes)] = plist_bytes

    out = bytes(data[:insert_point]) + bytes(appended) + bytes(data[insert_point:])

    with open(out_path, "wb") as f:
        f.write(out)

    sys.stderr.write("prelink_merge: insert_point=0x%x insert_size=0x%x (kext=0x%x plist=0x%x)\n" %
                      (insert_point, insert_size, kext_padded, plist_padded))
    sys.stderr.write("prelink_merge: __PRELINK_TEXT vmaddr=0x%x fileoff=0x%x size=0x%x (real=%d)\n" %
                      (new_pt_vmaddr, new_pt_fileoff, kext_padded, len(kext_bytes)))
    sys.stderr.write("prelink_merge: __PRELINK_INFO vmaddr=0x%x fileoff=0x%x size=0x%x (real=%d)\n" %
                      (new_pi_vmaddr, new_pi_fileoff, plist_padded, len(plist_bytes)))
    sys.stderr.write("prelink_merge: wrote %s (%d bytes, was %d)\n" % (out_path, len(out), len(data)))
    sys.stderr.write("prelink_merge: target vmaddr for kxld_link_tool: %x\n" % new_pt_vmaddr)


if __name__ == "__main__":
    main()
