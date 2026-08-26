/* See notify.h -- honest inert stub, no real notifyd in this project. */
#include <notify.h>
#include <stddef.h>

uint32_t
notify_register_check(const char *name, int *out_token)
{
	(void)name;
	if (out_token != NULL) *out_token = -1;
	return NOTIFY_STATUS_FAILED;
}

uint32_t
notify_register_plain(const char *name, int *out_token)
{
	(void)name;
	if (out_token != NULL) *out_token = -1;
	return NOTIFY_STATUS_FAILED;
}

uint32_t
notify_cancel(int token)
{
	(void)token;
	return NOTIFY_STATUS_OK;
}

uint32_t
notify_check(int token, int *check)
{
	(void)token;
	if (check != NULL) *check = 0;
	return NOTIFY_STATUS_FAILED;
}

uint32_t
notify_get_state(int token, uint64_t *state)
{
	(void)token;
	if (state != NULL) *state = 0;
	return NOTIFY_STATUS_FAILED;
}

uint32_t
notify_monitor_file(int token, const char *path, int flags)
{
	(void)token; (void)path; (void)flags;
	return NOTIFY_STATUS_FAILED;
}
