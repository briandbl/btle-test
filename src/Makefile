#
# Makefile for ble-test demo
#

BTSTACK_ROOT = ./btstack

CC      = gcc
CFLAGS  = -O0 -Wall -I. -I${BTSTACK_ROOT}/src -I${BTSTACK_ROOT}/include 
LDFLAGS = -lusb-1.0 

CORE   = \
    ${BTSTACK_ROOT}/src/btstack_memory.c          \
    ${BTSTACK_ROOT}/src/linked_list.c	          \
    ${BTSTACK_ROOT}/src/memory_pool.c             \
    ${BTSTACK_ROOT}/src/run_loop.c		          \
    ${BTSTACK_ROOT}/src/run_loop_posix.c

COMMON = \
    ${BTSTACK_ROOT}/src/hci.c			          \
    ${BTSTACK_ROOT}/src/hci_cmds.c		          \
    ${BTSTACK_ROOT}/src/hci_dump.c		          \
    ${BTSTACK_ROOT}/src/hci_transport_usb.c 	  \
    ${BTSTACK_ROOT}/src/remote_device_db_memory.c \
    ${BTSTACK_ROOT}/src/utils.c			          \
    ${BTSTACK_ROOT}/src/att.c    	              \
    ${BTSTACK_ROOT}/src/l2cap_le.c                \
	
CORE_OBJ   = $(CORE:.c=.o)
COMMON_OBJ = $(COMMON:.c=.o)

# create demo from common objects and example source file
all: ble-test
	
# compile .gatt description
profile.h: profile.gatt
	${BTSTACK_ROOT}/tools/compile-gatt.py $< $@ 
    
ble-test: profile.h ble-test.o ${CORE_OBJ} ${COMMON_OBJ} 
	${CC} $^ ${LDFLAGS} -o $@


clean:
	rm -f ble-test profile.h *.o ${BTSTACK_ROOT}/src/*.o 

