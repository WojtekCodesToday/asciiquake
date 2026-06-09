#include "quakedef.h"
#include "net_udp.h"

// Keep global linkage slots open so the engine links correctly
static int net_acceptsocket = -1;
static int net_controlsocket = -1;
static int net_broadcastsocket = 0;
static struct qsockaddr broadcastaddr;
static unsigned long myAddr = 0;

int UDP_Init (void)
{
    // Return failure instantly. Quake will gracefully slide right back into local loopback mode.
    return -1;
}

void UDP_Shutdown (void) {}
void UDP_Listen (qboolean state) {}
int UDP_OpenSocket (int port) { return -1; }
int UDP_CloseSocket (int socket) { return -1; }
int UDP_Connect (int socket, struct qsockaddr *addr) { return 0; }
int UDP_CheckNewConnections (void) { return -1; }
int UDP_Read (int socket, byte *buf, int len, struct qsockaddr *addr) { return 0; }
int UDP_MakeSocketBroadcastCapable (int socket) { return -1; }
int UDP_Broadcast (int socket, byte *buf, int len) { return -1; }
int UDP_Write (int socket, byte *buf, int len, struct qsockaddr *addr) { return 0; }
char *UDP_AddrToString (struct qsockaddr *addr) { return "127.0.0.1:26000"; }
int UDP_StringToAddr (char *string, struct qsockaddr *addr) { return 0; }
int UDP_GetSocketAddr (int socket, struct qsockaddr *addr) { return 0; }
int UDP_GetNameFromAddr (struct qsockaddr *addr, char *name) { return 0; }
int UDP_GetAddrFromName(char *name, struct qsockaddr *addr) { return -1; }
int UDP_AddrCompare (struct qsockaddr *addr1, struct qsockaddr *addr2) { return 0; }
int UDP_GetSocketPort (struct qsockaddr *addr) { return 26000; }
int UDP_SetSocketPort (struct qsockaddr *addr, int port) { return 0; }