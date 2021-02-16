#ifndef __DCF77_DECODER_H__
#define __DCF77_DECODER_H__
#include "hardware/pio.h"
#include "hardware/rtc.h"
#include "ssd1306.h"

class DCF77Time
{
    public:
        DCF77Time();
        ~DCF77Time();
        void reset();
        void add_bit(bool data);
        bool parity_ok();
        void convert(datetime_t * date_time);
    private:
        static bool parity(bool *raw, uint8_t start, uint8_t count, uint8_t parity);
        uint decode(uint8_t start, uint8_t count);
        bool raw_data[60];
        uint second_tick;
};

class DCF77Decoder
{
    public:
        DCF77Decoder();
        ~DCF77Decoder();
        void initialise();
        void run();
    private:
        void report(datetime_t &date_time, uint bits, uint errors, uint parity_errors);
        void init_gpio();
        void start_rx();
    private:
        PIO pio_0;
        uint rx_pin;
        uint led_pin;
        uint sync_pin;
        uint pdn_pin;
        datetime_t date_time;
        uint sm1;
        uint sm2;
        DCF77Time time_data;
        uint bits;
        uint errors;
        uint parity_errors;
        SSD1306 display;
};

#endif