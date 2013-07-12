
#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "openshift.h"


/* Init to defaults; override only if IP is set; */
static const char* openshift_loopback_name_ = "127.0.0.1";
static uint32_t openshift_loopback_addr_ = (uint32_t) 0x7f000001;

static pthread_once_t openshift_loopback_addr_init_guard_ = PTHREAD_ONCE_INIT;


static void openshift_loopback_addr_init(void) {
  const char* name;
  const uint32_t addr;

  name = getenv("IP");
  if (name == NULL) {
    /* Env var not found - stick to 127.0.0.1. */
    return;
  }

  if (!inet_pton(AF_INET, name, (void*) &addr)) {
    /* Env var was not a valid IP - stick to 127.0.0.1. */
    return;
  }

  /* Convert to machine byte order. */
  openshift_loopback_addr_ = ntohl(addr);

  openshift_loopback_name_ = strdup(name);
  if (openshift_loopback_name_ == NULL) {
    /* Out of memory */
    abort();
  }
}


uint32_t openshift_loopback_addr(void) {
  pthread_once(&openshift_loopback_addr_init_guard_,
               openshift_loopback_addr_init);

  return openshift_loopback_addr_;
}

const char* openshift_loopback_name(void) {
  pthread_once(&openshift_loopback_addr_init_guard_,
               openshift_loopback_addr_init);

  return openshift_loopback_name_;
}

