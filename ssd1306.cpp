#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
 
SSD1306::SSD1306(uint width, uint height): iWidth(width),iHeight(height)
{
#ifdef USE_DMA
    update_command_buffer = new CommandBuffer;
    uint32_t update_commands[] = { 0x00, 0x221,
                                   0x00, 0x200,
                                   0x00, 0x200 | iWidth-1,
                                   0x00, 0x222,
                                   0x00, 0x200,
                                   0x00, 0x200 | iHeight/8 - 1};
    for(int i=0;i<sizeof(update_commands);i++)
    {
        update_command_buffer->buffer[i] = update_commands[i];
    }
#endif

   screen_buffer = new uint8_t[iWidth*(iHeight/8)];
}

SSD1306::~SSD1306()
{
#ifdef USE_DMA
    delete update_command_buffer;
#endif
    delete screen_buffer;
}

int SSD1306::write_command(uint8_t byte)
{
    uint8_t buffer[2] = {0x00,byte};
    return i2c_write_timeout_us(i2c0, 0x3c, buffer, 2,false, 1000000);
}

int SSD1306::write_data(uint8_t *data, uint8_t len)
{
    uint8_t buffer[256];
    buffer[0] = 0x40;
    memcpy(buffer+1, data, len);
    return i2c_write_timeout_us(i2c0, 0x3c, buffer, 1+len, false, 1000000);
}

void SSD1306::initialise()
{
    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);

    initialise_screen();

#ifdef USE_DMA
    initialise_dma();
#endif
    set_screen(0x00);
    update_screen();
}

void SSD1306::initialise_screen()
{
    uint8_t commands[] = {0xAe, // disp off
             0xD5, // clk div
             0x80, // suggested ratio
             0xA8, 0x1F, // set multiplex: 0x1F for 128x32, 0x3F for 128x64
             0xD3,0x0, // display offset
             0x40, // start line
             0x8D,0x14, // charge pump
             0x20,0x0, // memory mode
             0xA1, // seg remap 1 
             0xC8, // comscandec
             0xDA,0x02, // set compins - use 0x12 for 128x64
             0x81,0xCF, // set contrast
             0xD9,0xF1, // set precharge
             0xDb,0x40, // set vcom detect
             0xA4, // display all on
             0xA6, // display normal (non-inverted)
             0xAf // disp on
    };
    for(int i=0;i<sizeof(commands);i++)
        write_command(commands[i]);
}

void SSD1306::initialise_dma()
{
    idma_tx = dma_claim_unused_channel(true);
    iConfig = dma_channel_get_default_config(idma_tx);
    channel_config_set_transfer_data_size(&iConfig, DMA_SIZE_32);
    channel_config_set_write_increment(&iConfig,false); // dont inc i2c address
    channel_config_set_read_increment(&iConfig,true);
    channel_config_set_dreq(&iConfig, DREQ_I2C0_TX);
}

void SSD1306::update_screen()
{
#ifdef USE_DMA
    if(dma_channel_is_busy (idma_tx))
    {
        printf("\x1b[10;0HDMA %d Busy....",idma_tx);
        dma_channel_wait_for_finish_blocking(idma_tx);
    }
    else
    {
        printf("\x1b[10;0HDMA %d Not Busy", idma_tx);
    }
    uint screen_buffer_pos = 12;
    uint8_t* src = screen_buffer;
    uint32_t* dst = update_command_buffer->buffer;
    dst[screen_buffer_pos++] = 0x40;
    for(int i=0;i<(iWidth*iHeight)/8;i++)
    {
        dst[screen_buffer_pos++] = src[i] & 0xff;
    }

    dst[screen_buffer_pos - 1] |= 0x200;

    uint items = screen_buffer_pos;
    volatile void * wr = (void*)(&i2c0->hw->data_cmd);
    dma_channel_configure(idma_tx, &iConfig,
                          wr,                            // write address
                          update_command_buffer->buffer, // read address
                          items,                         // element count (each element is of size transfer_data_size)
                          true);                         // Go!
#else
        write_command(0x21);
        write_command(0x00);
        write_command(iWidth - 1);
        write_command(0x22);
        write_command(0x00);
        write_command(iHeight/8 - 1);
        for(int j=0;j<iHeight/8;j++)
        {
            write_data(&screen_buffer[iWidth*j], iWidth);
        }
#endif
}

void SSD1306::draw_char(uint8_t x, uint8_t y, uint8_t chr)
{
    const uint8_t* chr_ptr = &ssd1306xled_font6x8[6*(chr - 32)];
    uint8_t* scr_ptr = screen_buffer + x + (128*y);
    for(int i=0;i<6;i++)
    {
        *(scr_ptr+i) = *(chr_ptr+i);
    }
}

void SSD1306::draw_str(uint8_t x, uint8_t y, const char* chr)
{
    while(*chr)
    {
        draw_char(x, y, *chr++);
        x += 6;
    }
}

void SSD1306::plot_pixel(uint8_t x, uint8_t y, uint8_t colour)
{
    if(colour)
        screen_buffer[x + (y / 8) * iWidth] |= 1 << (y % 8);
    else
        screen_buffer[x + (y / 8) * iWidth] &= ~(1 << (y % 8));
}

void SSD1306::horizontal_line(uint8_t x, uint8_t y, uint8_t width, uint8_t colour)
{
    uint8_t* start = x + &screen_buffer[(y / 8) * iWidth];
    uint8_t* end = width + start;
    if(colour)
    {
        uint8_t mask = 1 << (y % 8);
        for(uint8_t* p = start;p<end;p++)
        {
            *p |= mask;
        }
    }
    else
    {
        uint8_t mask = ~(1 << (y % 8));
        for(uint8_t* p = start;p<end;p++)
        {
            *p &= mask;
        }
    }
}

void SSD1306::horizontal_full_line(uint8_t y, uint8_t colour)
{
    horizontal_line(0,y, 128,colour);
}

void SSD1306::vertical_line(uint8_t x, uint8_t y, uint8_t colour)
{
    screen_buffer[x + y*128] = colour;
}

void SSD1306::fill_rect(uint8_t tx, uint8_t ty, uint8_t bx, uint8_t by, uint8_t colour)
{
    for(int i =tx + ty*128;i<bx + by*128;i++)
    {
        screen_buffer[i] = colour;
    }
    
}

void SSD1306::set_screen(uint8_t colour)
{
    for(int i=0;i<iWidth*iHeight/8;i++)
        screen_buffer[i] = colour;
}
