#ifndef NODE_PLATFORM_WIN32_WINSOCK_H_
#define NODE_PLATFORM_WIN32_WINSOCK_H_

#include <platform_win32.h>

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <ws2spi.h>

namespace node {

/*
 * Guids and typedefs for winsock extension functions
 * Mingw32 doesn't have these :-(
 */
#ifndef WSAID_ACCEPTEX
  const GUID WSAID_ACCEPTEX =
        {0xb5367df1, 0xcbac, 0x11cf, {0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}};

  const GUID WSAID_CONNECTEX =
        {0x25a207b9, 0xddf3, 0x4660, {0x8e, 0xe9, 0x76, 0xe5, 0x8c, 0x74, 0x06, 0x3e}};

  const GUID WSAID_GETACCEPTEXSOCKADDRS =
        {0xb5367df2, 0xcbac, 0x11cf, {0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}};

  const GUID WSAID_DISCONNECTEX =
        {0x7fda2e11, 0x8630, 0x436f, {0xa0, 0x31, 0xf5, 0x36, 0xa6, 0xee, 0xc1, 0x57}};

  const GUID WSAID_TRANSMITFILE =
        {0xb5367df0, 0xcbac, 0x11cf, {0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}};

  typedef BOOL(*LPFN_ACCEPTEX)
              (SOCKET sListenSocket,
               SOCKET sAcceptSocket,
               PVOID lpOutputBuffer,
               DWORD dwReceiveDataLength,
               DWORD dwLocalAddressLength,
               DWORD dwRemoteAddressLength,
               LPDWORD lpdwBytesReceived,
               LPOVERLAPPED lpOverlapped);

  typedef BOOL(*LPFN_CONNECTEX)
              (SOCKET s,
               const struct sockaddr *name,
               int namelen,
               PVOID lpSendBuffer,
               DWORD dwSendDataLength,
               LPDWORD lpdwBytesSent,
               LPOVERLAPPED lpOverlapped);

  typedef void(*LPFN_GETACCEPTEXSOCKADDRS)
              (PVOID lpOutputBuffer,
               DWORD dwReceiveDataLength,
               DWORD dwLocalAddressLength,
               DWORD dwRemoteAddressLength,
               LPSOCKADDR *LocalSockaddr,
               LPINT LocalSockaddrLength,
               LPSOCKADDR *RemoteSockaddr,
               LPINT RemoteSockaddrLength);

  typedef BOOL(*LPFN_DISCONNECTEX)
              (SOCKET hSocket,
               LPOVERLAPPED lpOverlapped,
               DWORD dwFlags,
               DWORD reserved);

  typedef BOOL(*LPFN_TRANSMITFILE)
              (SOCKET hSocket,
               HANDLE hFile,
               DWORD nNumberOfBytesToWrite,
               DWORD nNumberOfBytesPerSend,
               LPOVERLAPPED lpOverlapped,
               LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers,
               DWORD dwFlags);
#endif


/*
 * Pointers to winsock extension functions that have to be retrieved dynamically
 */
extern LPFN_CONNECTEX            pConnectEx;
extern LPFN_ACCEPTEX             pAcceptEx;
extern LPFN_GETACCEPTEXSOCKADDRS pGetAcceptExSockAddrs;
extern LPFN_DISCONNECTEX         pDisconnectEx;
extern LPFN_TRANSMITFILE         pTransmitFile;


void wsa_init();

void wsa_perror(const char* prefix = "");

SOCKET wsa_sync_socket(int af, int type, int proto);

int wsa_socketpair(int af, int type, int proto, SOCKET sock[2]);
int wsa_sync_async_socketpair(int af, int type, int proto, SOCKET *syncSocket, SOCKET *asyncSocket);


} // namespace node

#endif  // NODE_PLATFORM_WIN32_WINSOCK_H_
