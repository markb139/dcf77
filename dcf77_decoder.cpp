#include "stdio.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "dcf77.pio.h"
extern "C" {
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
} //extern
#include "dcf77_decoder.h"

const char day[][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char month[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

DCF77Time::DCF77Time(){
    this->second_tick = 0;
}
DCF77Time::~DCF77Time(){}

void DCF77Time::reset()
{
    this->second_tick = 0;
}

void DCF77Time::add_bit(bool data)
{
    this->raw_data[this->second_tick] = data;
    if (this->second_tick < 59)
    {
        this->second_tick++;
    }
    else
    {
        this->second_tick=0;
    }

}

bool DCF77Time::parity(bool *raw, uint8_t start, uint8_t count, uint8_t parity)
{
    uint bit_count = 0;
    for(int i=0;i<count;i++)
    {
        if(raw[start+i])
            bit_count++;
    }
    return raw[parity] == (bit_count & 1);
}

bool DCF77Time::parity_ok()
{
    return parity(this->raw_data, 21, 7, 28) & 
           parity(this->raw_data, 29, 6, 35) & 
           parity(this->raw_data, 36, 22, 58);
}

uint DCF77Time::decode(uint8_t start, uint8_t count)
{
    const uint8_t mult[] = {1,2,4,8,10,20,40,80};
    uint ret = 0;
    for(int i=0;i<count;i++)
    {
        ret += mult[i] * ((uint)this->raw_data[start+i]);
    }
    return ret;
}

void DCF77Time::convert(datetime_t * date_time)
{
    date_time->year = 2000 + this->decode(50, 8);
    date_time->month = this->decode(45, 5);
    date_time->day = this->decode(36, 6);
    date_time->dotw = this->decode(42, 3);
    date_time->hour = this->decode(29, 6);
    date_time->min = this->decode(21, 7);
    date_time->sec = 0;
}



DCF77Decoder::DCF77Decoder(): pio_0(pio0), led_pin(25), rx_pin(15), sync_pin(14), pdn_pin(16), display(128,32)
{
    this->date_time.year = 1970;
    this->date_time.month = 01;
    this->date_time.day = 01;
    this->date_time.dotw = 4; // 0 is Sunday, so 5 is Friday
    this->date_time.hour = 12;
    this->date_time.min = 00;
    this->date_time.sec = 00;
}

DCF77Decoder::~DCF77Decoder()
{}

void DCF77Decoder::init_gpio()
{
    gpio_init(this->pdn_pin);
    gpio_init(this->led_pin);
    gpio_init(this->sync_pin);
    
    gpio_set_dir(this->led_pin, GPIO_OUT);
    gpio_set_dir(this->pdn_pin, GPIO_OUT);
    gpio_set_dir(this->sync_pin, GPIO_OUT);
}

void DCF77Decoder::start_rx()
{
    gpio_put(this->led_pin,1);
    gpio_put(this->pdn_pin,1);
    gpio_put(this->sync_pin, 1);
    sleep_ms(1000);
    gpio_put(this->led_pin,0);
    gpio_put(this->pdn_pin,0);
    gpio_put(this->sync_pin,0);
}

void DCF77Decoder::initialise()
{
    display.initialise();
    this->bits = 0;
    this->errors = 0;
    this->parity_errors = 0;
    this->init_gpio();
    rtc_init();
    rtc_set_datetime(&this->date_time);
   
    this->sm1 = pio_claim_unused_sm(this->pio_0, true);
    this->sm2 = pio_claim_unused_sm(this->pio_0, true);
    uint offset = pio_add_program(this->pio_0, &pulse_low_program);
    pulse_low_program_init(this->pio_0, this->sm1, offset, this->rx_pin);

    offset = pio_add_program(this->pio_0, &pulse_hi_program);
    pulse_hi_program_init(this->pio_0, this->sm2, offset,this->rx_pin);
}

void DCF77Decoder::report(datetime_t &date_time, uint bits, uint errors, uint parity_errors)
{
    char datetime_buf[256];
    char *datetime_str = &datetime_buf[0];
    rtc_get_datetime(&date_time);
    sprintf(datetime_buf, "%s %d %s    ",day[date_time.dotw],date_time.day,month[date_time.month-1]);
    display.draw_str(0, 0, datetime_str);
    sprintf(datetime_buf, "%02d:%02d:%02d",date_time.hour,date_time.min,date_time.sec);
    display.draw_str(0, 1, datetime_str);
    if(date_time.sec == 0)
    {
        display.fill_rect(0, 3, 60, 3, 0x00);
    }
    else
    {
        display.vertical_line(date_time.sec, 3, 0xff);
    }
    display.update_screen();


    datetime_to_str(datetime_str, sizeof(datetime_buf), &date_time);
    
    printf("\x1b[0;0HDate Time: %s     ", datetime_str);
    printf("\x1b[5;0HBits: %d    \nErrors: %d    \nRate: %.2f%%    \nParity Errors: %d    \n", 
                 bits, errors, (float)errors/bits, parity_errors);
}

void DCF77Decoder::run()
{
    uint32_t low_time;
    uint32_t hi_time;
    bool failed=false;

    this->start_rx();

    while(true)
    {
        if(!pio_sm_is_rx_fifo_empty(this->pio_0, this->sm1))
        {
            low_time = ~pio_sm_get_blocking(this->pio_0, this->sm1);
            gpio_put(this->led_pin,1);
        }
        if(!pio_sm_is_rx_fifo_empty(this->pio_0, this->sm2))
        {
            hi_time = ~pio_sm_get_blocking(this->pio_0, this->sm2);
            if(low_time + hi_time > 900000)
            {
                gpio_put(this->led_pin,0);
                if(low_time > 75000 && low_time < 125000)
                {
                    //this->raw_data[second_tick] = 0;
                    this->time_data.add_bit(false);
                    bits++;
                }
                else if(low_time >= 175000 && low_time < 225000)
                {
                    // this->raw_data[second_tick] = 1;
                    this->time_data.add_bit(true);
                    bits++;
                }
                else
                {
                    gpio_put(this->sync_pin,1);
                    failed = true;
                    errors++;
                }
            }
            else
            {
                gpio_put(this->sync_pin,1);
                failed = true;
                errors++;
            }

            rtc_get_datetime(&this->date_time);
            this->report(this->date_time, this->bits, this->errors, this->parity_errors);

            if (hi_time > 1750000) 
            {
                gpio_put(this->sync_pin,0);
                if(!failed)
                {
                    if(this->time_data.parity_ok())
                    {
                        this->time_data.convert(&this->date_time);
                        rtc_set_datetime(&this->date_time);
                        this->report(this->date_time, this->bits, this->errors, this->parity_errors);
                    }
                    else
                        this->parity_errors++;
                }
                this->time_data.reset();
                failed = false;
            }
        }
    }
}