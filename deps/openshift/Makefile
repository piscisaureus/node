CFLAGS	=-Wall -Wextra

BUILDTYPE	?= Release

ifeq ($(BUILDTYPE),Release)
	CFLAGS	+= -O3
else
	CFLAGS	+= -O0 -g
endif

.PHONY:	all clean

all:	openshift.o

clean:
	@rm -f *.o *.a

openshift.o: openshift.c
	$(CC) $(CFLAGS) -o openshift.o -c openshift.c

