

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <time.h>
#include <errno.h>

#include "core/types.h"
#include "core/debug.h"
#include "core/io.h"

#include "device/ibmpc/rtc.h"

#include "util/bcd.h"
#include "util/fs.h"



void rtc_init(RTC *rtc)
{

	memset(rtc->nvram, 0, sizeof(rtc->nvram));

	rtc->reg = 0;
	rtc->gmt = false;

	rtc->nvram[RTC_REGB] = 0x00;
	rtc->nvram[RTC_REGC] = 0x00;
	rtc->nvram[RTC_REGD] = 0x80;

}



void rtc_tick(RTC *rtc)
{

// TODO: IRQs: periodic, alarm, update
// TODO: Update flag

	const bool enabled    = (rtc->nvram[RTC_REGB] & 0x80) == 0;
	const bool period_int = (rtc->nvram[RTC_REGB] & 0x40) != 0;
	const bool alarm_int  = (rtc->nvram[RTC_REGB] & 0x20) != 0;
	const bool update_int = (rtc->nvram[RTC_REGB] & 0x10) != 0;
	const bool bcd_mode   = (rtc->nvram[RTC_REGB] & 0x04) == 0;
	const bool is_24h     = (rtc->nvram[RTC_REGB] & 0x02) != 0;

	if (enabled) {

		time_t     now = time(NULL);
		struct tm *tm  = rtc->gmt? gmtime(&now): localtime(&now);

		if (!is_24h) {
			if      (tm->tm_hour == 0)  tm->tm_hour  = 12;
			else if (tm->tm_hour >  12) tm->tm_hour -= 12;
		}

		if (tm->tm_year >= 100)
			tm->tm_year -= 100;

		tm->tm_mon++;

		rtc->nvram[RTC_TIME_SECONDS] = bcd_mode? dec2bcd(tm->tm_sec):  tm->tm_sec;
		rtc->nvram[RTC_TIME_MINUTES] = bcd_mode? dec2bcd(tm->tm_min):  tm->tm_min;
		rtc->nvram[RTC_TIME_HOURS]   = bcd_mode? dec2bcd(tm->tm_hour): tm->tm_hour;

		rtc->nvram[RTC_DATE_WEEKDAY] = tm->tm_wday + 1;
		rtc->nvram[RTC_DATE_DAY]     = bcd_mode? dec2bcd(tm->tm_mday): tm->tm_mday;
		rtc->nvram[RTC_DATE_MONTH]   = bcd_mode? dec2bcd(tm->tm_mon):  tm->tm_mon;
		rtc->nvram[RTC_DATE_YEAR]    = bcd_mode? dec2bcd(tm->tm_year): tm->tm_year;

		rtc->nvram[RTC_REGA] &= ~0x80;

	}

}



void rtc_load(RTC *rtc, const char *path)
{

	fs_load(path, "CMOS NVRAM", rtc->nvram, sizeof(rtc->nvram), 1);

}



void rtc_save(RTC *rtc, const char *path)
{

	fs_save(path, "CMOS NVRAM", rtc->nvram, sizeof(rtc->nvram), 1);

}



void rtc_io_rd(RTC *rtc, u16 port, uint mode, uint *value)
{

	switch (port) {

		case 0x70: *value = rtc->reg;             break;
		case 0x71: *value = rtc->nvram[rtc->reg]; break;

	}

}



void rtc_io_wr(RTC *rtc, u16 port, uint mode, uint *value)
{

	switch (port) {

		case 0x70: rtc->reg                                    = *value; break;
		case 0x71: rtc->nvram[rtc->reg & (RTC_NVRAM_SIZE - 1)] = *value; break;

	}

}

