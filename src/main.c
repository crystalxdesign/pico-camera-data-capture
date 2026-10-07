#include <stdio.h>
#include <tusb.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "cam.h"
#include "LCD_1in14_V2.h" 
#include "GUI_Paint.h"
// Camera source dimensions are deliberately explicit: update alongside adapter.
#define CAMERA_W 324
#define CAMERA_H 324
#define OUT_W 32
#define OUT_H 32
uint8_t image_buf[CAMERA_W * CAMERA_H];
uint8_t displayBuf[240*135*2];
uint8_t header[2] = {0x55,0xAA};
static uint8_t reduced[OUT_W * OUT_H];
struct cam_config config;

#define FLAG_VALUE 666
uint8_t imageReady = 0;

void core1_entry() {

    // Notify core 0 that core 1 is ready
    multicore_fifo_push_blocking(FLAG_VALUE);

    // Wait for acknowledgment from core 0
    uint32_t ack  = multicore_fifo_pop_blocking();
    if (ack != FLAG_VALUE)
    	printf("Error: Core 1 failed to receive acknowledgment from core 0!\n");
	else
		printf("Success: Core 1 Received acknowledgment from core 0!\n");

    // LCD Init
    DEV_Module_Init();
    LCD_1IN14_V2_Init(HORIZONTAL);
    LCD_1IN14_V2_Clear(BLACK);
    UDOUBLE Imagesize = LCD_1IN14_V2_HEIGHT * LCD_1IN14_V2_WIDTH * 2;
    UWORD *BlackImage;
    if ((BlackImage = (UWORD *)malloc(Imagesize)) == NULL)
    {
        printf("Failed to apply for black memory...\r\n");
        exit(0);
    }
    
    // Create a new image cache and draw on the image
    Paint_NewImage((UBYTE *)BlackImage, LCD_1IN14_V2.WIDTH, LCD_1IN14_V2.HEIGHT, 0, WHITE);
    Paint_SetScale(65);
    Paint_SetRotate(ROTATE_0);
    LCD_1IN14_V2_Display(BlackImage);
    DEV_Delay_ms(500);

    // CAM Init
    struct cam_config config;
    cam_config_struct(&config);
    cam_init(&config);
    // Image Processing
    while (true) {
        cam_capture_frame(&config);

        uint16_t index = 0;
        for (int y = 134; y > 0; y--) {
            for (int x = 0; x < 240; x++) {
                uint16_t c = image_buf[(y)*324+(x)];
                uint16_t imageRGB = (((c & 0xF8) << 8) | ((c & 0xFC) << 3) | ((c & 0xF8) >> 3));
                displayBuf[index++] = (uint16_t)(imageRGB >> 8) & 0xFF;
                displayBuf[index++] = (uint16_t)(imageRGB) & 0xFF;
            }
        }

        // Set the imageReady flag to indicate the image is ready for display
        imageReady = 1;
    }
}

static void resize_area(void) {
    // Area average, no cropping; matches PC-side full-frame resize.
    for (int y=0; y<OUT_H; ++y) for(int x=0; x<OUT_W; ++x) {
        int x0=x*CAMERA_W/OUT_W, x1=(x+1)*CAMERA_W/OUT_W;
        int y0=y*CAMERA_H/OUT_H, y1=(y+1)*CAMERA_H/OUT_H;
        uint32_t sum=0, n=0;
        for(int yy=y0; yy<y1; ++yy) for(int xx=x0; xx<x1; ++xx) {sum+=image_buf[yy*CAMERA_W+xx];++n;}
        reduced[y*OUT_W+x]=(uint8_t)(sum/n);
    }
}

static void send_frame(void) {
    // AA 55 + 16-bit little-endian payload length + raw pixels + XOR checksum.
    const uint8_t header[4]={0xAA,0x55,0x00,0x04};
    uint8_t crc=0;
    for(int i=0;i<4;++i) putchar_raw(header[i]);
    for(int i=0;i<OUT_W*OUT_H;++i) {putchar_raw(reduced[i]);crc^=reduced[i];}
    putchar_raw(crc);
    fflush(stdout);
}

int main(void) {
    int loops = 20;
    stdio_init_all();
    while (!tud_cdc_connected()) {
        DEV_Delay_ms(100);
        if (--loops == 0)
            break;
    }

    printf("tud_cdc_connected(%d)\n", tud_cdc_connected() ? 1 : 0);
    vreg_set_voltage(VREG_VOLTAGE_1_10);
    set_sys_clock_khz(250000, true);
    multicore_launch_core1(core1_entry);
    // Wait for acknowledgment from core 1
    uint32_t ack = multicore_fifo_pop_blocking();
    if (ack != FLAG_VALUE)
        printf("Error: Core 0 failed to receive acknowledgment from core 1!\n");
    else {
        multicore_fifo_push_blocking(FLAG_VALUE);
        printf("Success: Core 0 Received acknowledgment from core 1!\n");
    }

    while(true) {
        if (imageReady == 1) {
            LCD_1IN14_V2_Display((uint16_t*)displayBuf);
            // Reset the imageReady flag after displaying the image
            imageReady = 0;
            // Host sends 'C' for one frame; 'I' to query camera status.
            int ch=getchar_timeout_us(10000);
            if(ch==PICO_ERROR_TIMEOUT) continue;
            if(ch=='I') { putchar_raw('1'); fflush(stdout); }
            if(ch=='C') {
                resize_area();
                send_frame();
            }
        }

    }
}
