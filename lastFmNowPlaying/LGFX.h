#ifndef LGFX_H
#define LGFX_H

#define LGFX_USE_V1
#include <HTTPClient.h>  // this lib needs to be before LovyanGFX, otherwise drawJpgUrl and drawPngUrl is unusable

#include <LovyanGFX.hpp>

// Waveshare ESP32-S3-Touch-LCD-2.8
// Remember to enable CNC on boot

// Font defined in main .ino (after U8g2lib.h) so the symbol is available in one
// place only.
extern const lgfx::U8g2font myExtendedFont;
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;

   public:
    LGFX(void) {
        {  // SPI
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;

            cfg.pin_sclk = 40;
            cfg.pin_mosi = 45;
            cfg.pin_miso = -1;
            cfg.pin_dc = 41;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {  // LCD Panel
            auto cfg = _panel_instance.config();
            cfg.pin_cs = 42;
            cfg.pin_rst = 39;
            cfg.pin_busy = -1;

            cfg.panel_width = 240;
            cfg.panel_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false cfg.dlen_16bit = false;
            cfg.bus_shared = true

                             _panel_instance.config(cfg);
        }

        {  // Backlight
            auto cfg = _light_instance.config();
            cfg.pin_bl = 5;
            cfg.invert = false;
            cfg.freq = 12000;
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

#endif