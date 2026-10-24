#pragma once
#include <memo/kernel.h>
struct rtc_time { int year, month, day, hour, min, sec; };
void rtc_read(struct rtc_time *t);
