/* Minimal os_log shim -- real Darwin's unified logging doesn't exist in
 * this project at all. Every vendored Phase 25 (SystemConfiguration/
 * configd) source file that calls os_log_*()/SC_log() gets a plain
 * printf-based substitute here instead: same call shape (so the real
 * vendored .c files need no source edits for logging calls specifically),
 * output goes to the console like every other diagnostic in this project
 * (PTHREADTEST/DISPATCHTEST/... all just printf). Not a functional gap
 * that matters here -- os_log's real value (structured, queryable,
 * privacy-redacted logs) has no consumer in this project either. */
#ifndef _OS_LOG_H_
#define _OS_LOG_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct os_log_s *os_log_t;

#define OS_LOG_DEFAULT ((os_log_t)0)

static inline os_log_t os_log_create(const char *subsystem, const char *category)
{
	(void)subsystem; (void)category;
	return (os_log_t)1; /* non-NULL sentinel; nothing dereferences it */
}

#define os_log_debug(log, fmt, ...) do { (void)(log); printf("[debug] " fmt "\n", ##__VA_ARGS__); } while (0)
#define os_log_info(log, fmt, ...) do { (void)(log); printf("[info] " fmt "\n", ##__VA_ARGS__); } while (0)
#define os_log(log, fmt, ...) do { (void)(log); printf(fmt "\n", ##__VA_ARGS__); } while (0)
#define os_log_error(log, fmt, ...) do { (void)(log); printf("[error] " fmt "\n", ##__VA_ARGS__); } while (0)
#define os_log_fault(log, fmt, ...) do { (void)(log); printf("[fault] " fmt "\n", ##__VA_ARGS__); } while (0)

#ifdef __cplusplus
}
#endif

#endif /* _OS_LOG_H_ */
