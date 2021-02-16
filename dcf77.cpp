#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "dcf77_decoder.h"

int main()
{
    stdio_init_all();

    DCF77Decoder decoder;
    decoder.initialise();
    decoder.run();
    while(true)
    {
        sleep_ms(10);
    }
}