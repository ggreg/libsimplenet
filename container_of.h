#ifndef _CONTAINER_OF_H_
#define _CONTAINER_OF_H_ 1


#include <stddef.h>

#define container_of(ptr, type, member) ({ \
                const typeof( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - offsetof(type,member) );})

#endif
/* vim: ts=8:sw=8:noet
*/
