/*
 * snmp_vars.c - return a pointer to the named variable.
 *
 *
 */
/***********************************************************
	Copyright 1988, 1989, 1990 by Carnegie Mellon University
	Copyright 1989	TGV, Incorporated

		      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU and TGV not be used
in advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

CMU AND TGV DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL CMU OR TGV BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
******************************************************************/
/*
 * additions, fixes and enhancements for Linux by Erik Schoenfelder
 * (schoenfr@ibr.cs.tu-bs.de) 1994/1995.
 * Linux additions taken from CMU to UCD stack by Jennifer Bray of Origin
 * (jbray@origin-at.co.uk) 1997
 */


#include <config.h>
#if STDC_HEADERS
#include <string.h>
#include <stdlib.h>
#else
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>

#include <sys/socket.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#include <netinet/ip.h>
#if HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif
#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if HAVE_NET_ROUTE_H
#include <net/route.h>
#endif
#if HAVE_NETINET_IN_PCB_H
#include <netinet/in_pcb.h>
#endif
#if HAVE_INET_MIB2_H
#include <inet/mib2.h>
#endif

#include "mibincl.h"
#include "snmpv3.h"
#include "snmpusm.h"
/* #include "agentx.h" */
#include "m2m.h"
#include "snmp_vars_m2m.h"
#include "../snmplib/system.h"
#include "kernel.h"
#include "snmp_vars.h"

#include "mibgroup/struct.h"
#include "mibgroup/mib_module_includes.h"
#include "read_config.h"
#include "snmp_vars.h"
#include "agent_read_config.h"
#include "mib_module_config.h"
#if USING_MIBII_VACM_VARS_MODULE
#include "mibgroup/mibII/vacm_vars.h"
#endif

#include "snmpd.h"

#ifndef  MIN
#define  MIN(a,b)                     (((a) < (b)) ? (a) : (b)) 
#endif

extern struct subtree *subtrees;
int subtree_size;
int subtree_malloc_size;

/*
 *	Each variable name is placed in the variable table, without the
 * terminating substring that determines the instance of the variable.  When
 * a string is found that is lexicographicly preceded by the input string,
 * the function for that entry is called to find the method of access of the
 * instance of the named variable.  If that variable is not found, NULL is
 * returned, and the search through the table continues (it will probably
 * stop at the next entry).  If it is found, the function returns a character
 * pointer and a length or a function pointer.  The former is the address
 * of the operand, the latter is a write routine for the variable.
 *
 * u_char *
 * findVar(name, length, exact, var_len, write_method)
 * oid	    *name;	    IN/OUT - input name requested, output name found
 * int	    length;	    IN/OUT - number of sub-ids in the in and out oid's
 * int	    exact;	    IN - TRUE if an exact match was requested.
 * int	    len;	    OUT - length of variable or 0 if function returned.
 * int	    write_method;   OUT - pointer to function to set variable,
 *                                otherwise 0
 *
 *     The writeVar function is returned to handle row addition or complex
 * writes that require boundary checking or executing an action.
 * This routine will be called three times for each varbind in the packet.
 * The first time for each varbind, action is set to RESERVE1.  The type
 * and value should be checked during this pass.  If any other variables
 * in the MIB depend on this variable, this variable will be stored away
 * (but *not* committed!) in a place where it can be found by a call to
 * writeVar for a dependent variable, even in the same PDU.  During
 * the second pass, action is set to RESERVE2.  If this variable is dependent
 * on any other variables, it will check them now.  It must check to see
 * if any non-committed values have been stored for variables in the same
 * PDU that it depends on.  Sometimes resources will need to be reserved
 * in the first two passes to guarantee that the operation can proceed
 * during the third pass.  During the third pass, if there were no errors
 * in the first two passes, writeVar is called for every varbind with action
 * set to COMMIT.  It is now that the values should be written.  If there
 * were errors during the first two passes, writeVar is called in the third
 * pass once for each varbind, with the action set to FREE.  An opportunity
 * is thus provided to free those resources reserved in the first two passes.
 * 
 * writeVar(action, var_val, var_val_type, var_val_len, statP, name, name_len)
 * int	    action;	    IN - RESERVE1, RESERVE2, COMMIT, or FREE
 * u_char   *var_val;	    IN - input or output buffer space
 * u_char   var_val_type;   IN - type of input buffer
 * int	    var_val_len;    IN - input and output buffer len
 * u_char   *statP;	    IN - pointer to local statistic
 * oid      *name           IN - pointer to name requested
 * int      name_len        IN - number of sub-ids in the name
 */

long		long_return;
#ifndef ibm032
u_char		return_buf[258];  
#else
u_char		return_buf[256]; /* nee 64 */
#endif


void
init_agent (void)
{
#ifdef CAN_USE_NLIST
	init_kmem("/dev/kmem");
#endif

	setup_tree();

#include "mibgroup/mib_module_inits.h"

	init_agent_read_config();

#ifdef TESTING
	auto_nlist_print_tree(-2, 0);
#endif
}  /* end init_agent() */


#define CMUMIB 1, 3, 6, 1, 4, 1, 3
#define       CMUUNIXMIB  CMUMIB, 2, 2

#define SNMPV2 			1, 3, 6, 1, 6
#define SNMPV2M2M		SNMPV2, 3, 2

#define SNMPV2ALARMEVENTS	SNMPV2M2M, 1, 1, 3

#define RMONMIB 1, 3, 6, 1, 2, 1, 16

#define HOST                    RMONMIB, 4
#define HOSTCONTROL             HOST, 1, 1                      /* hostControlEntry */
#define HOSTTAB                 HOST, 2, 1                      /* hostEntry */
#define HOSTTIMETAB             HOST, 3, 1                      /* hostTimeEntry */
#define HOSTTOPN                RMONMIB, 5
#define HOSTTOPNCONTROL HOSTTOPN, 1, 1          /* hostTopNControlEntry */
#define HOSTTOPNTAB             HOSTTOPN, 2, 1          /* hostTopNEntry */
#define HOSTTIMETABADDRESS                                      1
#define HOSTTIMETABCREATIONORDER                        2
#define HOSTTIMETABINDEX                                        3
#define HOSTTIMETABINPKTS                                       4
#define HOSTTIMETABOUTPKTS                                      5
#define HOSTTIMETABINOCTETS                                     6
#define HOSTTIMETABOUTOCTETS                            7
#define HOSTTIMETABOUTERRORS                            8
#define HOSTTIMETABOUTBCASTPKTS                         9
#define HOSTTIMETABOUTMCASTPKTS                         10

#if 0
#define RMONMIB 1, 3, 6, 1, 2, 1, 16

#define ALARM                   RMONMIB, 3
#define ALARMTAB                ALARM, 1, 1                 /* alarmEntry */
#define EVENT                   RMONMIB, 9
#define EVENTTAB                EVENT, 1, 1                 /* eventEntry */
#endif


/* various OIDs that are needed throughout the agent */
#ifdef USING_V2PARTY_ALARM_MODULE
Export oid alarmVariableOid[] = {SNMPV2ALARMENTRY, ALARMTABVARIABLE};
Export int alarmVariableOidLen = sizeof(alarmVariableOid) / sizeof(oid);
Export oid alarmSampleTypeOid[] = {SNMPV2ALARMENTRY, ALARMTABSAMPLETYPE};
Export int alarmSampleTypeOidLen = sizeof(alarmSampleTypeOid) / sizeof(oid);
Export oid alarmValueOid[] = {SNMPV2ALARMENTRY, ALARMTABVALUE};
Export int alarmValueOidLen = sizeof(alarmValueOid) / sizeof(oid);
Export oid alarmFallingThreshOid[] = {SNMPV2ALARMENTRY, ALARMTABFALLINGTHRESH};
Export int alarmFallingThreshOidLen = sizeof(alarmFallingThreshOid)/sizeof(oid);
Export oid alarmRisingThreshOid[] = {SNMPV2ALARMENTRY, ALARMTABRISINGTHRESH};
Export int alarmRisingThreshOidLen = sizeof(alarmRisingThreshOid)/sizeof(oid);
#endif

Export oid nullOid[] = {0,0};
Export int nullOidLen = sizeof(nullOid)/sizeof(oid);
Export oid sysUpTimeOid[] = {1,3,6,1,2,1,1,3,0};
Export int sysUpTimeOidLen = sizeof(sysUpTimeOid)/sizeof(oid);
#ifdef USING_V2PARTY_EVENT_MODULE
Export oid eventIdOid[] = {SNMPV2EVENTENTRY, EVENTTABID};
Export int eventIdOidLen = sizeof(eventIdOid)/sizeof(oid);
Export oid trapRisingAlarmOid[] = {SNMPV2ALARMEVENTS, 1};
Export int trapRisingAlarmOidLen = sizeof(trapRisingAlarmOid)/sizeof(oid);
Export oid trapFallingAlarmOid[] = {SNMPV2ALARMEVENTS, 2};
Export int trapFallingAlarmOidLen = sizeof(trapFallingAlarmOid)/sizeof(oid);
Export oid trapObjUnavailAlarmOid[] = {SNMPV2ALARMEVENTS, 3};
Export int trapObjUnavailAlarmOidLen = sizeof(trapObjUnavailAlarmOid)/sizeof(oid);
#endif


/*
 * getStatPtr - return a pointer to the named variable, as well as it's
 * type, length, and access control list.
 * Now uses 'search_subtree' (recursively) and 'search_subtree_vars'
 * to do most of the work
 *
 * If an exact match for the variable name exists, it is returned.  If not,
 * and exact is false, the next variable lexicographically after the
 * requested one is returned.
 *
 * If no appropriate variable can be found, NULL is returned.
 */
static  int 		found;

static u_char *
search_subtree_vars(struct subtree *tp,
		    oid *name,    /* IN - name of var, OUT - name matched */
		    size_t *namelen, /* IN -number of sub-ids in name,
                                     OUT - subid-is in matched name */
		    u_char *type, /* OUT - type of matched variable */
		    size_t *len,  /* OUT - length of matched variable */
		    u_short *acl, /* OUT - access control list */
		    int exact,    /* IN - TRUE if exact match wanted */
		    WriteMethod **write_method,
		    struct snmp_pdu *pdu, /* IN - relevant auth info re PDU */
		    int *noSuchObject)
{
    register struct variable *vp;
    struct variable	compat_var, *cvp = &compat_var;
    register int	x;
    u_char		*access = NULL;
    int			result;
    oid 		*suffix;
    size_t		suffixlen;

	    if ( tp->variables == NULL )
		return NULL;

	    result = compare_tree(name, *namelen, tp->name, tp->namelen);
	    suffixlen = *namelen - tp->namelen;
	    suffix = name + tp->namelen;
	    /* the following is part of the setup for the compatability
	       structure below that has been moved out of the main loop.
	     */
	    memcpy(cvp->name, tp->name, tp->namelen * sizeof(oid));

	    for(x = 0, vp = tp->variables; x < tp->variables_len;
		vp =(struct variable *)((char *)vp +tp->variables_width), x++){
		/* if exact and ALWAYS
		   if next  and result >= 0 */
                /* and if vp->namelen != 0   -- Wes */
		if (vp->namelen && (exact || result >= 0)){
		    result = compare_tree(suffix, suffixlen, vp->name,
				     vp->namelen);
		}
		/* if exact and result == 0
		   if next  and result <= 0 */
                /* or if vp->namelen == 0    -- Wes */
		if ((!exact && (result <= 0)) || (exact && (result == 0)) ||
                  vp->namelen == 0) {
		    /* builds an old (long) style variable structure to retain
		       compatability with var_* functions written previously.
		     */
                  if (vp->namelen)
                    memcpy((cvp->name + tp->namelen),
			  vp->name, vp->namelen * sizeof(oid));
		    cvp->namelen = tp->namelen + vp->namelen;
		    cvp->type = vp->type;
		    cvp->magic = vp->magic;
		    cvp->acl = vp->acl;
		    cvp->findVar = vp->findVar;
                    *write_method = NULL;
		    access = (*(vp->findVar))(cvp, name, namelen, exact,
						  len, write_method);
		    if (*write_method)
			*acl = cvp->acl;
                    /* check for permission to view this part of the OID tree */
		    if ((access != NULL || (*write_method != NULL && exact)) &&
                        !in_a_view(name, namelen, pdu, cvp->type)) {
                        access = NULL;
			*write_method = NULL;
		    } else if (exact){
			found = TRUE;
		    }
		    /* this code is incorrect if there is
		       a view configuration that exludes a particular
		       instance of a variable.  It would return noSuchObject,
		       which would be an error */
		    if (access != NULL || (*write_method != NULL && exact))
			break;
		}
		/* if exact and result <= 0 */
		if (exact && (result  <= 0)){
	            *type = cvp->type;
		    *acl = cvp->acl;
		    if (found)
                      *noSuchObject = FALSE;
		    else
                      *noSuchObject = TRUE;
		    return NULL;
		}
	    }
	    if (access != NULL || (exact && *write_method != NULL)) {
	        *type = cvp->type;
		*acl = cvp->acl;
		return access;
	    }
	    return NULL;
}

u_char *
getStatPtr(
    oid		*name,	    /* IN - name of var, OUT - name matched */
    size_t	*namelen,   /* IN -number of sub-ids in name,
                               OUT - subid-is in matched name */
    u_char	*type,	    /* OUT - type of matched variable */
    size_t	*len,	    /* OUT - length of matched variable */
    u_short	*acl,	    /* OUT - access control list */
    int		exact,	    /* IN - TRUE if exact match wanted */
    WriteMethod **write_method,
    struct snmp_pdu *pdu,   /* IN - relevant auth info re PDU */
    int		*noSuchObject)
{
    struct subtree	*tp, *prev;
    oid			save[MAX_OID_LEN];
    size_t		savelen = 0;
    u_char              result_type;
    u_short             result_acl;
    u_char        *search_return=NULL;

    found = FALSE;

    if (!exact){
	memcpy(save, name, *namelen * sizeof(oid));
	savelen = *namelen;
    }
    *write_method = NULL;
    for (tp = subtrees, prev=NULL; tp != NULL ; prev=tp, tp = tp->next ) {
      if ( snmp_oid_compare( tp->name, tp->namelen, name, *namelen ) >= 0 )
          break;
    }
    if ( tp != NULL && prev != NULL &
       snmp_oid_compare( tp->name, tp->namelen, name, *namelen ) > 0 )
              tp = prev;
/*
    tp = find_subtree_previous(name, *namelen, NULL);
    if ( tp->next &&
         snmp_oid_compare( tp->next->name, tp->next->namelen,
			   name, *namelen ) == 0 )
	tp = tp->next;
*/
 
    while ( search_return == NULL && tp != NULL ) {
	search_return = search_subtree_vars( tp, name, namelen, &result_type,
                                        len, &result_acl, exact, write_method,
                                        pdu, noSuchObject);
	if ( search_return != NULL || (*write_method != NULL && exact))
	    break;
	tp = tp->next;
    }
    if ( tp == NULL ) {
	if (!search_return && !exact){
	    memcpy(name, save, savelen * sizeof(oid));
	    *namelen = savelen;
	}
	if (found)
	    *noSuchObject = FALSE;
	else
	    *noSuchObject = TRUE;
        return NULL;
    }
    *type = result_type;
    *acl =  result_acl;
    return search_return;
}

