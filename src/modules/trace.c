/*
 *   IRC - Internet Relay Chat, src/modules/m_trace.c
 *   (C) 2004 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

CMD_FUNC(m_trace);

#define MSG_TRACE 	"TRACE"	

ModuleHeader MOD_HEADER(trace)
  = {
	"trace",
	"5.0",
	"command /trace", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(trace)
{
	CommandAdd(modinfo->handle, MSG_TRACE, m_trace, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(trace)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(trace)
{
	return MOD_SUCCESS;
}

/*
** m_trace
**	parv[1] = servername
*/
CMD_FUNC(m_trace)
{
	int  i;
	aClient *acptr;
	ConfigItem_class *cltmp;
	char *tname;
	int  doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	int  cnt = 0, wilds, dow;
	time_t now;

	// XXX: FIXME: This would be one of those asynch/multi-server
	// commands that cannot be handled by labeled-response!
	// set some kind of flag? or?

	if (parc > 2)
		if (hunt_server(cptr, sptr, NULL, ":%s TRACE %s :%s", 2, parc, parv))
			return 0;

	if (parc > 1)
		tname = parv[1];
	else
		tname = me.name;

	if (!ValidatePermissionsForPath("client:see:trace:global",sptr,NULL,NULL,NULL))
	{
		if (ValidatePermissionsForPath("client:see:trace:local",sptr,NULL,NULL,NULL))
		{
			/* local opers may not /TRACE remote servers! */
			if (strcasecmp(tname, me.name))
			{
				sendnotice(sptr, "You can only /TRACE local servers as a locop");
				sendnumeric(sptr, ERR_NOPRIVILEGES);
				return 0;
			}
		} else {
			sendnumeric(sptr, ERR_NOPRIVILEGES);
			return 0;
		}
	}

	switch (hunt_server(cptr, sptr, NULL, ":%s TRACE :%s", 1, parc, parv))
	{
	  case HUNTED_PASS:	/* note: gets here only if parv[1] exists */
	  {
		  aClient *ac2ptr;

		  ac2ptr = find_client(tname, NULL);
		  sendnumeric(sptr, RPL_TRACELINK,
		      version, debugmode, tname, ac2ptr->from->name);
		  return 0;
	  }
	  case HUNTED_ISME:
		  break;
	  default:
		  return 0;
	}

	doall = (parv[1] && (parc > 1)) ? !match_simple(tname, me.name) : TRUE;
	wilds = !parv[1] || index(tname, '*') || index(tname, '?');
	dow = wilds || doall;

	for (i = 0; i < MAXCONNECTIONS; i++)
		link_s[i] = 0, link_u[i] = 0;


	if (doall) {
		list_for_each_entry(acptr, &client_list, client_node)
		{
			if (acptr->from->fd < 0)
				continue;
#ifdef	SHOW_INVISIBLE_LUSERS
			if (IsPerson(acptr))
				link_u[acptr->from->fd]++;
#else
			if (IsPerson(acptr) &&
			    (!IsInvisible(acptr) || ValidatePermissionsForPath("client:see:trace:invisible-users",sptr,acptr,NULL,NULL)))
				link_u[acptr->from->fd]++;
#endif
			else if (IsServer(acptr))
				link_s[acptr->from->fd]++;
		}
	}

	/* report all direct connections */

	now = TStime();
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		char *name;
		char *class;

		if (!ValidatePermissionsForPath("client:see:trace:invisible-users",sptr,acptr,NULL,NULL) && (acptr != sptr))
			continue;
		if (!doall && wilds && match_simple(tname, acptr->name))
			continue;
		if (!dow && mycmp(tname, acptr->name))
			continue;
		name = get_client_name(acptr, FALSE);
		class = acptr->local->class ? acptr->local->class->name : "default";
		switch (acptr->status)
		{
			case STAT_CONNECTING:
				sendnumeric(sptr, RPL_TRACECONNECTING, class, name);
				cnt++;
				break;

			case STAT_HANDSHAKE:
				sendnumeric(sptr, RPL_TRACEHANDSHAKE, class, name);
				cnt++;
				break;

			case STAT_ME:
				break;

			case STAT_UNKNOWN:
				sendnumeric(sptr, RPL_TRACEUNKNOWN, class, name);
				cnt++;
				break;

			case STAT_CLIENT:
				/* Only opers see users if there is a wildcard
				 * but anyone can see all the opers.
				 */
				if (ValidatePermissionsForPath("client:see:trace:invisible-users",sptr,acptr,NULL,NULL) ||
				    (!IsInvisible(acptr) && ValidatePermissionsForPath("client:see:trace",sptr,acptr,NULL,NULL)))
				{
					if (ValidatePermissionsForPath("client:see:trace",sptr,acptr,NULL,NULL) || ValidatePermissionsForPath("client:see:trace:invisible-users",sptr,acptr,NULL,NULL))
						sendnumeric(sptr, RPL_TRACEOPERATOR,
						    class, acptr->name,
						    GetHost(acptr),
						    now - acptr->local->lasttime);
					else
						sendnumeric(sptr, RPL_TRACEUSER,
						    class, acptr->name,
						    acptr->user->realhost,
						    now - acptr->local->lasttime);
					cnt++;
				}
				break;

			case STAT_SERVER:
				if (acptr->serv->user)
					sendnumeric(sptr, RPL_TRACESERVER, class, acptr->fd >= 0 ? link_s[acptr->fd] : -1,
					    acptr->fd >= 0 ? link_u[acptr->fd] : -1, name, acptr->serv->by,
					    acptr->serv->user->username,
					    acptr->serv->user->realhost,
					    now - acptr->local->lasttime);
				else
					sendnumeric(sptr, RPL_TRACESERVER, class, acptr->fd >= 0 ? link_s[acptr->fd] : -1,
					    acptr->fd >= 0 ? link_u[acptr->fd] : -1, name, *(acptr->serv->by) ?
					    acptr->serv->by : "*", "*", me.name,
					    now - acptr->local->lasttime);
				cnt++;
				break;

			case STAT_LOG:
				sendnumeric(sptr, RPL_TRACELOG, LOGFILE, acptr->local->port);
				cnt++;
				break;

			case STAT_TLS_CONNECT_HANDSHAKE:
				sendnumeric(sptr, RPL_TRACENEWTYPE, "TLS-Connect-Handshake", name);
				cnt++;
				break;

			case STAT_TLS_ACCEPT_HANDSHAKE:
				sendnumeric(sptr, RPL_TRACENEWTYPE, "TLS-Accept-Handshake", name);
				cnt++;
				break;

			default:	/* ...we actually shouldn't come here... --msa */
				sendnumeric(sptr, RPL_TRACENEWTYPE, "<newtype>", name);
				cnt++;
				break;
		}
	}
	/*
	 * Add these lines to summarize the above which can get rather long
	 * and messy when done remotely - Avalon
	 */
	if (!ValidatePermissionsForPath("client:see:trace",sptr,acptr,NULL,NULL) || !cnt)
		return 0;

	for (cltmp = conf_class; doall && cltmp; cltmp = cltmp->next)
	/*	if (cltmp->clients > 0) */
			sendnumeric(sptr, RPL_TRACECLASS, cltmp->name ? cltmp->name : "[noname]", cltmp->clients);
	return 0;
}