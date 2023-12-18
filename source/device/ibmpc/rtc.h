

#ifndef DEVICE_RTC_H
#define DEVICE_RTC_H


enum {
	RTC_NVRAM_SIZE = 128
};

enum {

	RTC_TIME_SECONDS = 0,
	RTC_TIME_MINUTES = 2,
	RTC_TIME_HOURS   = 4,

	RTC_DATE_WEEKDAY = 6,
	RTC_DATE_DAY     = 7,
	RTC_DATE_MONTH   = 8,
	RTC_DATE_YEAR    = 9,

	RTC_ALARM_SECONDS = 1,
	RTC_ALARM_MINUTES = 3,
	RTC_ALARM_HOURS   = 5,

	RTC_REGA = 10,
	RTC_REGB = 11,
	RTC_REGC = 12,
	RTC_REGD = 13

};


typedef struct {

	u8 nvram[RTC_NVRAM_SIZE];

	bool gmt;
	uint reg;

} RTC;


void rtc_init(RTC *rtc);
void rtc_tick(RTC *rtc);

void rtc_load(RTC *rtc, const char *file);
void rtc_save(RTC *rtc, const char *file);

void rtc_io_rd(RTC *rtc, u16 port, uint mode, uint *value);
void rtc_io_wr(RTC *rtc, u16 port, uint mode, uint *value);


static inline struct io rtc_mkport(RTC *rtc) {
	return io_make(rtc, (io_fn*)&rtc_io_rd, (io_fn*)&rtc_io_wr);
}


#endif

