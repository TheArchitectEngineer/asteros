/* See dnsinfo.h -- honest stub. Real libresolv's own res_init() already
 * falls back to /etc/resolv.conf when this returns NULL. */
#include <dnsinfo.h>
#include <stddef.h>

dns_config_t *
dns_configuration_copy(void)
{
	return NULL;
}

void
dns_configuration_free(dns_config_t *config)
{
	(void)config;
}
