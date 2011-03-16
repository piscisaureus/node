
extern "C" {

#include "ev.h"
#include <stdio.h>

HANDLE iocp;
IocpPacket *free_iocp_packet_list = NULL;

ev_prepare *ev_prepare_list[EV_NUMPRI];
ev_check *ev_check_list[EV_NUMPRI];
ev_idle *ev_idle_list[EV_NUMPRI];

ev_tstamp ev_rt_now;

typedef struct _OVERLAPPED_ENTRY {
  ULONG_PTR    lpCompletionKey;
  LPOVERLAPPED lpOverlapped;
  ULONG_PTR    Internal;
  DWORD        dwNumberOfBytesTransferred;
} OVERLAPPED_ENTRY, *LPOVERLAPPED_ENTRY;

BOOL WINAPI GetQueuedCompletionStatusEx(
  HANDLE CompletionPort,
  LPOVERLAPPED_ENTRY lpCompletionPortEntries,
  ULONG ulCount,
  PULONG ulNumEntriesRemoved,
  DWORD dwMilliseconds,
  BOOL fAlertable
);



void iocp_fatal_error(const char *syscall) {
  DWORD errorno = GetLastError();
  char *errmsg = NULL;

  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorno,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (CHAR*)&errmsg, 0, NULL);
  if (!errmsg) {
    errmsg = strdup("Unknown error\n");
  }

  fprintf(stderr, "Event loop died with fatal error\n");
  if (syscall) {
    fprintf(stderr, "%s: (%d) %s", syscall, errorno, errmsg);
  } else {
    fprintf(stderr, "(%d) %s", errorno, errmsg);
  }


  free(errmsg);

  *((char*)NULL) = 1; /* Force gdb debug break */
  abort();
}

ev_tstamp ev_time (void)
{
  FILETIME ft;
  ULARGE_INTEGER ui;

  GetSystemTimeAsFileTime (&ft);
  ui.u.LowPart  = ft.dwLowDateTime;
  ui.u.HighPart = ft.dwHighDateTime;

  /* msvc cannot convert ulonglong to double... yes, it is that sucky */
  return (LONGLONG)(ui.QuadPart - 116444736000000000) * 1e-7;
}


void iocp_init() {
  iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (iocp == NULL) {
    iocp_fatal_error("CreateIoCompletionPort");
  }

  memset(&ev_prepare_list, 0, sizeof(ev_prepare_list));
  memset(&ev_check_list, 0, sizeof(ev_check_list));
  memset(&ev_idle_list, 0, sizeof(ev_idle_list));
}

#define ev_invoke_static(type, abs_pri, events)                 \
  do {                                                          \
    for (type *w = type##_list[abs_pri]; w; w = w->next) {      \
      w->cb(w, events);                                         \
    }                                                           \
  } while (0)

void ev_async_handle_packet(IocpPacket *packet) {
  ev_async *w = packet->w_async;

  if (w == NULL) {
    /* The watcher has been stopped while this packet was in the queue. */
    /* Free the packet, don't call any callbacks */
    FreeIocpPacket(packet);
  } else {
    /* Clear the sent status and call the watcher's callback */
    /* Don't release the packet, it will be released only after the watcher */
    /* is stopped. */
    w->sent = 0;
    w->cb(w, EV_ASYNC);
  }
}


void CALLBACK ev_timer_timeout_cb(void *data, BOOLEAN fired) {
  assert(fired);
  ev_timer *w = (ev_timer*)data;

  w->sent = 1;

  BOOL success = PostQueuedCompletionStatus(iocp, 0, 0, PacketToOverlapped(w->packet));
  if (!success)
    iocp_fatal_error("PostQueuedCompletionStatus");
}


void ev_timer_handle_packet(IocpPacket *packet) {
  ev_timer *w = packet->w_timer;

  if (w == NULL) {
    /* The watcher has been stopped while this packet was in the queue. */
    /* Free the packet, don't call any callbacks */
    FreeIocpPacket(packet);
  } else {
    /* Clear the sent status and call the watcher's callback */
    /* Don't release the packet, it will be released only after the watcher */
    /* is stopped. */
    w->sent = 0;
    w->cb(w, EV_TIMER);
  }
}

static inline void iocp_poll() {
  BOOL success;
  DWORD bytes;
  DWORD key;
  OVERLAPPED *overlapped;
  IocpPacket *packet;
  int called;


  for (int i = EV_NUMPRI; i >= 0; i--)
    ev_invoke_static(ev_prepare, i, EV_PREPARE);

  success = GetQueuedCompletionStatus(iocp,
                                      &bytes,
                                      &key,
                                      &overlapped,
                                      INFINITE);

  if (!success && !overlapped)
    iocp_fatal_error("GetQueuedCompletionStatus");

  packet = OverlappedToPacket(overlapped);

  called = 0;
  for (int i = EV_NUMPRI; i >= 0; i--) {
    ev_invoke_static(ev_check, i, EV_CHECK);

    if (packet && !called && EV_ABSPRI(packet->priority) == i) {
      called = 1;
      packet->callback(packet);
    }

    while (ev_idle_list[i])
      ev_invoke_static(ev_idle, i, EV_IDLE);
  }
}

void iocp_run(void) {
  ev_now_update();
  while (1)
    iocp_poll();
}

}
