/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// net_dgrm.c

// This is enables a simple IP banning mechanism

#include "quakedef.h"
#include "net_dgrm.h"
int			Datagram_Init (void){return 0;};
void		Datagram_Listen (qboolean state){};
void		Datagram_SearchForHosts (qboolean xmit){};
qsocket_t	*Datagram_Connect (char *host){return NULL;};
qsocket_t 	*Datagram_CheckNewConnections (void){return NULL;};
int			Datagram_GetMessage (qsocket_t *sock){return 1;};
int			Datagram_SendMessage (qsocket_t *sock, sizebuf_t *data){return 1;};
int			Datagram_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data){return 1;};
qboolean	Datagram_CanSendMessage (qsocket_t *sock){return true;};
qboolean	Datagram_CanSendUnreliableMessage (qsocket_t *sock){return true;};
void		Datagram_Close (qsocket_t *sock){};
void		Datagram_Shutdown (void){};