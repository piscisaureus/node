
#ifndef OPENSHIFT_H_
#define OPENSHIFT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <arpa/inet.h>
#include <stdint.h>


uint32_t openshift_loopback_addr(void);
const char* openshift_loopback_name(void);


/* Redefine INADDR_LOOPBACK to use the openshift loopback address. */
#undef INADDR_LOOPBACK
#define INADDR_LOOPBACK (openshift_loopback_addr())


#ifdef __cplusplus
}
#endif

#endif
