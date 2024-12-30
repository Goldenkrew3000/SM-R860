#ifndef _NTP_HANDLER_H
#define _NTP_HANDLER_H
#include <sys/types.h>

uint32_t ntp_fetchTime();
int ntp_setSystemTime(uint32_t epoch);
char* ntp_getTimeString();

#endif
