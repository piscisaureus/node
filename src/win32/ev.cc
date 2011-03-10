
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

void CALLBACK ev_timer_handle_apc(void *arg, DWORD timeLow, DWORD timeHigh) {
  ev_timer *w = (ev_timer*)arg;
  int called = 0;

  if (w->repeat <= 0)
    ev_timer_stop(w);

  for (int i = EV_NUMPRI; i >= 0; i--) {
    ev_invoke_static(ev_check, i, EV_CHECK);

    if (!called && EV_ABSPRI(w->priority) == i) {
      called = 1;
      w->cb(w, EV_TIMER);
    }

    while (ev_idle_list[i])
      ev_invoke_static(ev_idle, i, EV_IDLE);
  }

  for (int i = EV_NUMPRI; i >= 0; i--)
    ev_invoke_static(ev_prepare, i, EV_PREPARE);
}


void ev_async_handle_packet(IocpPacket *packet) {
  ev_async *w = packet->w_async;
  FreeIocpPacket(packet);
  /* Clear the sent status before invoking the watcher callback */
  w->sent = 0;
  if (w->active)
    w->cb(w, EV_ASYNC);
}

static inline void iocp_poll() {
  BOOL success;
  DWORD bytes;
  ULONG_PTR *key;
  OVERLAPPED_ENTRY overlapped_entry = {0};
  IocpPacket *packet;
  int called;


  for (int i = EV_NUMPRI; i >= 0; i--)
    ev_invoke_static(ev_prepare, i, EV_PREPARE);

//  success = GetQueuedCompletionStatus(iocp,
//                                      &bytes,
//                                      &key,
//                                      (LPOVERLAPPED*)packet);

  ULONG count = 0;
  success = GetQueuedCompletionStatusEx(
    iocp,
    &overlapped_entry,
    1,
    &count,
    INFINITE,
    TRUE
  );

  if (!success && !count)
    iocp_fatal_error("GetQueuedCompletionStatusEx");

  //if (!count || !overlapped_entry.lpOverlapped)
  //  return;

  packet = OverlappedToPacket(overlapped_entry.lpOverlapped);

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
