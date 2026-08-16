/* End-to-end smoke test for the AsterOS Objective-C cross-compilation
 * pipeline (see build/tools/asteros-sdk/bin/clang.cfg): a real class with
 * an ivar, a synthesized property, an instance method, and a class
 * method, built with `clang hello.m -o hello` and nothing else. `Object`
 * here is libobjc.A.dylib's real root class (Root.m) -- this file only
 * forward-declares the subset of its interface it uses, same as
 * userland/libobjc/test/test.m.
 */
#include <objc/objc.h>
#include <objc/runtime.h>
#include <stdio.h>

@interface Object
+ (id)alloc;
- (id)init;
- (Class)class;
@end

@interface Greeter : Object
{
	int _timesGreeted;
}
@property (nonatomic) int timesGreeted;
- (const char *)greet:(const char *)name;
@end

@implementation Greeter
@synthesize timesGreeted = _timesGreeted;

- (const char *)greet:(const char *)name
{
	static char buf[128];
	_timesGreeted++;
	snprintf(buf, sizeof(buf), "Hello, %s! (class=%s, greeting #%d)",
	    name, class_getName([self class]), _timesGreeted);
	return buf;
}
@end

int
main(void)
{
	Greeter *g = [[Greeter alloc] init];
	printf("%s\n", [g greet:"AsterOS"]);
	printf("%s\n", [g greet:"Objective-C"]);
	printf("timesGreeted property = %d\n", g.timesGreeted);
	printf("HELLO_OBJC PASS\n");
	return 0;
}
