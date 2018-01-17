
/* coap_config.h
 *
 * Copyright (C) 2012,2014 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see
 * README for terms of use. 
 */

#ifndef __COAP_CONFIG_H__
#define __COAP_CONFIG_H__

#include <rtthread.h>

// Defined in SConscript's CPPDEFINES
#ifdef WITH_POSIX
#include <sys/socket.h>

#define HAVE_MALLOC

#define IP_PKTINFO   IP_MULTICAST_IF
// #define IPV6_PKTINFO IPV6_V6ONLY

#define PACKAGE_NAME "libcoap"
#define PACKAGE_VERSION "@b425b150"

#define CUSTOM_COAP_NETWORK_ENDPOINT
#define CUSTOM_COAP_NETWORK_SEND
#define CUSTOM_COAP_NETWORK_READ

#endif

#define HAVE_STDIO_H
#define HAVE_ASSERT_H

#define PACKAGE_STRING PACKAGE_NAME PACKAGE_VERSION

// #define assert(x) LWIP_ASSERT("CoAP assert failed", x)

/* it's just provided by libc. i hope we don't get too many of those, as
 * actually we'd need autotools again to find out what environment we're
 * building in */
#define HAVE_STRNLEN 1

#define HAVE_LIMITS_H

#define COAP_RESOURCES_NOHASH

#endif /* _CONFIG_H_ */
