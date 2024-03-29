/* 
 * Copyright (C) 2007-2009  Samuel Vinson <samuelv0304@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/errno.h>
#include <pj/ip_helper.h>
#include <pj/compat/socket.h>

/*
#include <nds.h>
#include <dswifi9.h>
*/
PJ_DEF(pj_status_t) pj_gethostbyname(const pj_str_t *hostname, pj_hostent *phe)
{
    struct hostent *he;
    char copy[PJ_MAX_HOSTNAME];

    pj_assert(hostname && hostname ->slen < PJ_MAX_HOSTNAME);
    
    if (hostname->slen >= PJ_MAX_HOSTNAME)
	return PJ_ENAMETOOLONG;

    pj_memcpy(copy, hostname->ptr, hostname->slen);
    copy[ hostname->slen ] = '\0';

    he = gethostbyname(copy);
    if (!he) {
	return PJ_ERESOLVE;
	/* DO NOT use pj_get_netos_error() since host resolution error
	 * is reported in h_errno instead of errno!
	return pj_get_netos_error();
	 */
    }

    phe->h_name = he->h_name;
    phe->h_aliases = he->h_aliases;
    phe->h_addrtype = he->h_addrtype;
    phe->h_length = he->h_length;
    phe->h_addr_list = he->h_addr_list;

    return PJ_SUCCESS;
}

/* Resolve the IP address of local machine */
// FIXME remplacer par pj_getdefaultipinterface dans sock_common.c
/*
PJ_DEF(pj_status_t) pj_gethostip(pj_in_addr *addr)
{
    pj_assert(addr);
    addr->s_addr = Wifi_GetIP();
    return PJ_SUCCESS;
}
*/

/* Resolve IPv4/IPv6 address */
PJ_DEF(pj_status_t) pj_getaddrinfo(int af, const pj_str_t *nodename,
				   unsigned *count, pj_addrinfo ai[])
{
#if defined(PJ_SOCK_HAS_GETADDRINFO) && PJ_SOCK_HAS_GETADDRINFO!=0
    char nodecopy[PJ_MAX_HOSTNAME];
    struct addrinfo hint, *res, *orig_res;
    unsigned i;
    int rc;

    PJ_ASSERT_RETURN(nodename && count && *count && ai, PJ_EINVAL);
    PJ_ASSERT_RETURN(nodename->ptr && nodename->slen, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6 ||
		     af==PJ_AF_UNSPEC, PJ_EINVAL);

    /* Check if nodename is IP address */
    pj_bzero(&ai[0], sizeof(ai[0]));
    ai[0].ai_addr.addr.sa_family = (pj_uint16_t)af;
    if (pj_inet_pton(af, nodename, pj_sockaddr_get_addr(&ai[0].ai_addr)) 
   == PJ_SUCCESS) 
    {
  pj_str_t tmp;

  tmp.ptr = ai[0].ai_canonname;
  pj_strncpy_with_null(&tmp, nodename, PJ_MAX_HOSTNAME);
  ai[0].ai_addr.addr.sa_family = (pj_uint16_t)af;
  *count = 1;

  return PJ_SUCCESS;
    }

    /* Copy node name to null terminated string. */
    if (nodename->slen >= PJ_MAX_HOSTNAME)
	return PJ_ENAMETOOLONG;
    pj_memcpy(nodecopy, nodename->ptr, nodename->slen);
    nodecopy[nodename->slen] = '\0';

    /* Call getaddrinfo() */
    pj_bzero(&hint, sizeof(hint));
    hint.ai_family = af;

    rc = getaddrinfo(nodecopy, NULL, &hint, &res);
    if (rc != 0)
	return PJ_ERESOLVE;

    orig_res = res;

    /* Enumerate each item in the result */
    for (i=0; i<*count && res; res=res->ai_next) {
	/* Ignore unwanted address families */
	if (af!=PJ_AF_UNSPEC && res->ai_family != af)
	    continue;

	/* Store canonical name (possibly truncating the name) */
	if (res->ai_canonname) {
	    pj_ansi_strncpy(ai[i].ai_canonname, res->ai_canonname,
			    sizeof(ai[i].ai_canonname));
	    ai[i].ai_canonname[sizeof(ai[i].ai_canonname)-1] = '\0';
	} else {
	    pj_ansi_strcpy(ai[i].ai_canonname, nodecopy);
	}

	/* Store address */
	PJ_ASSERT_ON_FAIL(res->ai_addrlen <= sizeof(pj_sockaddr), continue);
	pj_memcpy(&ai[i].ai_addr, res->ai_addr, res->ai_addrlen);

	/* Next slot */
	++i;
    }

    *count = i;

    freeaddrinfo(orig_res);

    /* Done */
    return PJ_SUCCESS;

#else	/* PJ_SOCK_HAS_GETADDRINFO */

    PJ_ASSERT_RETURN(count && *count, PJ_EINVAL);

    /* Check if nodename is IP address */
    pj_bzero(&ai[0], sizeof(ai[0]));
    ai[0].ai_addr.addr.sa_family = (pj_uint16_t)af;
    if (pj_inet_pton(af, nodename, pj_sockaddr_get_addr(&ai[0].ai_addr)) 
   == PJ_SUCCESS) 
    {
  pj_str_t tmp;

  tmp.ptr = ai[0].ai_canonname;
  pj_strncpy_with_null(&tmp, nodename, PJ_MAX_HOSTNAME);
  ai[0].ai_addr.addr.sa_family = (pj_uint16_t)af;
  *count = 1;

  return PJ_SUCCESS;
    }

    if (af == PJ_AF_INET || af == PJ_AF_UNSPEC) {
	pj_hostent he;
	unsigned i, max_count;
	pj_status_t status;
	
	status = pj_gethostbyname(nodename, &he);
	if (status != PJ_SUCCESS)
	    return status;

	max_count = *count;
	*count = 0;

	pj_bzero(ai, max_count * sizeof(pj_addrinfo));

	for (i=0; he.h_addr_list[i] && *count<max_count; ++i) {
	    pj_ansi_strncpy(ai[*count].ai_canonname, he.h_name,
			    sizeof(ai[*count].ai_canonname));
	    ai[*count].ai_canonname[sizeof(ai[*count].ai_canonname)-1] = '\0';

	    ai[*count].ai_addr.ipv4.sin_family = PJ_AF_INET;
	    pj_memcpy(&ai[*count].ai_addr.ipv4.sin_addr,
		      he.h_addr_list[i], he.h_length);

	    (*count)++;
	}

	return PJ_SUCCESS;

    } else {
	/* IPv6 is not supported */
	*count = 0;

	return PJ_EIPV6NOTSUP;
    }
#endif	/* PJ_SOCK_HAS_GETADDRINFO */
}

