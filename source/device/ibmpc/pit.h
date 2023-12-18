

#ifndef DEVICE_PIT_H
#define DEVICE_PIT_H


enum {

	PIT_NUM_CHANNELS = 3,
	PIT_FREQUENCY    = 1193182

};


typedef struct {

	struct {

		uint count[2];

		uint cfg;
		uint period;
		uint freq;

		bool enabled;
		bool bcd;
		bool hilo;
		bool flip;
		bool status;

		struct wire config;
		struct wire output;

	} channel[PIT_NUM_CHANNELS];

} PIT;



void pit_init(    PIT *pit);
void pit_command( PIT *pit, uint cmd);

void pit_channel_reset( PIT *pit, uint ch);
void pit_channel_config(PIT *pit, uint ch, uint mode);
uint pit_channel_read(  PIT *pit, uint ch);
void pit_channel_write( PIT *pit, uint ch, uint value);
uint pit_channel_status(PIT *pit, uint ch);

void pit_io_rd(PIT *pit, u16 port, uint mode, uint *value);
void pit_io_wr(PIT *pit, u16 port, uint mode, uint *value);


static inline struct io pit_mkport(PIT *pit) {
	return io_make(pit, (io_fn*)&pit_io_rd, (io_fn*)&pit_io_wr);
}


#endif
