#ifndef LGFX_H
#define LGFX_H

#define LGFX_USE_V1
#include <HTTPClient.h> // this lib needs to be before LovyanGFX, otherwise drawJpgUrl and drawPngUrl is unusable
#include <LovyanGFX.hpp>

// Font defined in main .ino (after U8g2lib.h) so the symbol is available in one place only.
extern const lgfx::U8g2font myExtendedFont;
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789    _panel_instance;
    lgfx::Bus_SPI         _bus_instance;
    lgfx::Light_PWM       _light_instance;

public:
    LGFX(void)
    {
        { // Konfiguracja magistrali SPI
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;     // Użycie SPI2 (HSPI)
            cfg.spi_mode = 0;             // Tryb SPI 0
            cfg.freq_write = 40000000;    // Częstotliwość zapisu (40 MHz)
            cfg.freq_read  = 16000000;    // Częstotliwość odczytu
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            
            // Piny SPI dla Waveshare ESP32-S3-Touch-LCD-2.8
            cfg.pin_sclk = 40;  // SCK / SCLK
            cfg.pin_mosi = 45;  // MOSI / SDA
            cfg.pin_miso = -1;  // MISO (nie używane przez sam ekran)
            cfg.pin_dc   = 41;  // Data/Command (DC)
            
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        { // Konfiguracja panelu LCD
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = 42;  // Chip Select (CS)
            cfg.pin_rst  = 39;  // Reset (RST)
            cfg.pin_busy = -1;  // Busy

            cfg.panel_width  = 240;
            cfg.panel_height = 320;
            cfg.offset_x     = 0;
            cfg.offset_y     = 0;
            cfg.offset_rotation = 0;
            cfg.readable     = false;
            cfg.invert       = true;   // Często wymagane dla ST7789 na tej płytce (zmień na false, jeśli kolory będą negatywem)
            cfg.rgb_order    = false;  // false = RGB
            cfg.dlen_16bit   = false;
            cfg.bus_shared   = true;   // Wspólna magistrala (np. z kartą SD)

            _panel_instance.config(cfg);
        }

        { // Konfiguracja podświetlenia (Backlight)
            auto cfg = _light_instance.config();
            cfg.pin_bl = 5;       // Pin podświetlenia (BL)
            cfg.invert = false;
            cfg.freq   = 12000;
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

#endif