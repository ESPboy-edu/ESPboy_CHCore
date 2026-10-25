#pragma GCC optimize("Os")
#include "CHGfx.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "lib/ESPboyInit.h"
#include "lib/nbSPI.h"

extern ESPboyInit myESPboy;

// Global pointers for graphics buffers
uint8_t* gfx_fb = nullptr;
uint16_t gfx_pal[16];

static uint8_t* chunk_scratch = nullptr;

static uint16_t palette16[16];

// Two DMA buffers of 8 lines each (4 KB total instead of 32 KB for a full 16bpp frame)
#define CHUNK_LINES 8
static uint16_t* dma_buf[2] = {nullptr, nullptr};

void gfx_init() {
    // Allocate memory for all buffers strictly at initialization to prevent fragmentation
    if (!gfx_fb) gfx_fb = (uint8_t*)malloc(GFX_FB_BYTES);
    if (!chunk_scratch) chunk_scratch = (uint8_t*)malloc(1024);
    if (!dma_buf[0]) dma_buf[0] = (uint16_t*)malloc(GFX_W * CHUNK_LINES * 2);
    if (!dma_buf[1]) dma_buf[1] = (uint16_t*)malloc(GFX_W * CHUNK_LINES * 2);
}

// Hardware-specific stubs. 
// ESPboyInit already handles the display hardware initialization, so we ignore these.
void gfx_begin(uint8_t spiDiv, uint8_t colorMode) { (void)spiDiv; (void)colorMode; }
void gfx_setSpiDiv(uint8_t div) { (void)div; }
void gfx_setColorMode(uint8_t mode) { (void)mode; }
uint8_t gfx_colorMode(void) { return GFX_16BPP; }
uint8_t gfx_spiDiv(void) { return GFX_DIV2; }
uint32_t gfx_spiHz(void) { return 24000000; }
void gfx_setPanelOffsets(uint8_t madctl, uint8_t colStart, uint8_t rowStart) { (void)madctl; (void)colStart; (void)rowStart; }
void gfx_setInverted(bool on) { myESPboy.tft.invertDisplay(on); }
void gfx_setPanelFrameRate(uint8_t rtna, uint8_t fpa, uint8_t bpa) { (void)rtna; (void)fpa; (void)bpa; }

uint8_t* gfx_chunkScratch() {
    if (!chunk_scratch) chunk_scratch = (uint8_t*)malloc(1024);
    return chunk_scratch;
}

void gfx_setPalette(const uint16_t *colors, uint8_t n) {
    for(uint8_t i = 0; i < n && i < 16; i++) {
        uint16_t color565 = colors[i];
        // TFT_eSPI usually handles byte swapping, but nbSPI sends raw bytes.
        // Therefore, we swap the bytes of each palette color here.
        palette16[i] = (color565 >> 8) | (color565 << 8); 
    }
}
void gfx_setPaletteEntry(uint8_t index, uint16_t rgb565) {
    palette16[index & 0x0F] = (rgb565 >> 8) | (rgb565 << 8);
}
uint16_t gfx_paletteOut(uint8_t index) { return palette16[index & 0x0F]; }

void gfx_wait() {
    while(nbSPI_isBusy()) { yield(); }
}
bool gfx_busy() { return nbSPI_isBusy(); }

int  gfx_flushRow() { return nbSPI_isBusy() ? 0 : GFX_H; } // Simplified: if busy, assume drawing from top
void gfx_waitRow(int y) { (void)y; gfx_wait(); }

// Asynchronous chunk-based frame rendering
void gfx_flushAsync() {
    // Safety check for OOM (Out Of Memory)
    if (!gfx_fb || !dma_buf[0] || !dma_buf[1]) return;
    
    gfx_wait(); // Ensure SPI bus is free before starting
    
    myESPboy.tft.setWindow(0, 0, GFX_W - 1, GFX_H - 1);
    
    int fb_idx = 0;
    int buf_idx = 0;
    
    for (int y = 0; y < GFX_H; y += CHUNK_LINES) {
        uint16_t* ptr = dma_buf[buf_idx];
        
        // Unpack the 4-bit framebuffer into 16-bit colors for the current chunk
        for (int i = 0; i < (GFX_W * CHUNK_LINES) / 2; i++) {
            uint8_t pair = gfx_fb[fb_idx++];
            *ptr++ = palette16[pair & 0x0F];
            *ptr++ = palette16[pair >> 4];
        }
        
        gfx_wait(); // Wait for the previous chunk transmission to finish
        
        // Send the current chunk asynchronously via interrupt-driven SPI
        nbSPI_writeBytes((uint8_t*)dma_buf[buf_idx], GFX_W * CHUNK_LINES * 2);
        
        buf_idx ^= 1; // Swap double buffer index
    }
}

void gfx_flush() {
    gfx_flushAsync();
    gfx_wait();
}

// Stubs for partial updates (can be fully implemented with nbSPI later if needed)
void gfx_flushRect(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; gfx_flush(); }
void gfx_flushRectAsync(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; gfx_flushAsync(); }

// Direct blit stubs
void gfx_select(void) { }
void gfx_deselect(void) { }
void gfx_setWindow(uint8_t x, uint8_t y, uint8_t w, uint8_t h) { myESPboy.tft.setWindow(x, y, x+w-1, y+h-1); }
void gfx_cmd(uint8_t c) { (void)c; }
void gfx_data8(uint8_t d) { (void)d; }
void gfx_writeColorLut(void) { }
void gfx_setWriteColorLut(bool on) { (void)on; }
void gfx_directFillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t rgb565) { myESPboy.tft.fillRect(x, y, w, h, rgb565); }
void gfx_directBlit(const void *data, uint32_t bytes, bool halfword) { (void)halfword; nbSPI_writeBytes((uint8_t*)data, bytes); gfx_wait(); }
void gfx_blockingWrite(const uint8_t *data, uint32_t bytes) { nbSPI_writeBytes((uint8_t*)data, bytes); gfx_wait(); }
void gfx_stream(gfx_streamFn fn, void *user) { (void)fn; (void)user; } // Not used by casino games