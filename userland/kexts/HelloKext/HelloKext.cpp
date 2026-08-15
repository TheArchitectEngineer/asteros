/* Phase 23 validation kext: proves the boot-time prelinked-kext pipeline
 * end to end (real kxld link against the running kernel image, real
 * IOKitPersonalities-driven IOService matching) before Phase 24 attempts
 * the real virtio-net driver. Deliberately does nothing but log one line
 * from start() -- see TODO.md's Phase 23 writeup for the acceptance
 * criterion this exists to satisfy. */
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
extern "C" {
#include <mach/kmod.h>
extern void kprintf(const char *format, ...);
}

/* Every real kext needs a `kmod_info_t` global (classic pre-IOKit module
 * descriptor Darwin's kext-loading machinery is still built on) --
 * kxld_link_file() looks this symbol up by name and reports its final
 * linked address back as kmod_info_kern, which becomes this kext's
 * _PrelinkKmodInfoKey value. IOKit's own IOService::start()/stop() (not
 * these) drive this driver's real lifecycle; these two are vestigial
 * stubs, same as any pure-IOService-based kext's real ones. */
extern "C" kern_return_t
com_asteros_HelloKext_start(kmod_info_t *ki, void *data)
{
	(void)ki; (void)data;
	return KERN_SUCCESS;
}

extern "C" kern_return_t
com_asteros_HelloKext_stop(kmod_info_t *ki, void *data)
{
	(void)ki; (void)data;
	return KERN_SUCCESS;
}

/* Not KMOD_EXPLICIT_DECL: that macro stringifies its first argument
 * (#name) into kmod_info's name field, and a bare macro argument can't
 * contain the dots a real bundle identifier needs. Originally assumed
 * that didn't matter -- "the real bundle ID lives in Info.plist's
 * CFBundleIdentifier, which is what OSKext/IOKit actually match on, not
 * this cosmetic kmod_info name field" -- but that assumption was wrong,
 * caught live: OSRuntimeInitializeCPP()'s OSMetaClass::postModLoad() path
 * looks up the currently-loading kext via OSKext::lookupKextWithIdentifier
 * (kmod_info->name), which is keyed on the *real* bundle identifier
 * (registerIdentifier() stores kexts under CFBundleIdentifier, i.e. with
 * dots) -- so an underscored kmod name never matches, class registration
 * silently no-ops, and OSMetaClass::allocClassWithName() can never find
 * the class. Build the struct directly so name can hold the real,
 * dotted identifier. */
kmod_info_t kmod_info = {
	NULL, KMOD_INFO_VERSION, (uint32_t)-1,
	"com.asteros.HelloKext", "1.0.0", -1, NULL, 0, 0, 0,
	com_asteros_HelloKext_start, com_asteros_HelloKext_stop
};

class com_asteros_HelloKext : public IOService {
	OSDeclareDefaultStructors(com_asteros_HelloKext);

public:
	bool start(IOService * provider) override;
};

OSDefineMetaClassAndStructors(com_asteros_HelloKext, IOService);

bool
com_asteros_HelloKext::start(IOService * provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	kprintf("HelloKext: real prelinked kext loaded and started (kprintf)\n");
	IOLog("HelloKext: real prelinked kext loaded and started\n");
	return true;
}
