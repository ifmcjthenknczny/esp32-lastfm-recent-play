#ifndef LGFX_H
#define LGFX_H

#define LGFX_USE_V1
#include <HTTPClient.h> // this lib needs to be before LovyanGFX, otherwise drawJpgUrl and drawPngUrl is unusable
#include <LovyanGFX.hpp>

static const lgfx::U8g2font myExtendedFont(u8g2_font_unifont_t_extended);

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel_instance; // ST7789 panel
    lgfx::Bus_Parallel8 _bus_instance;  // MCU8080 8-bit parallel bus
    lgfx::Light_PWM _light_instance;    // Instance for backlight control

public:
    LGFX(void)
    {
        { // Bus settings configuration
            auto cfg = _bus_instance.config();
            cfg.freq_write = 25000000; // Write frequency
            cfg.pin_wr = 4;           // Write strobe pin
            cfg.pin_rd = 2;           // Read strobe pin
            cfg.pin_rs = 16;          // Register Select (DC / Data/Command) pin

            // Data pins D0-D7
            cfg.pin_d0 = 15;
            cfg.pin_d1 = 13;
            cfg.pin_d2 = 12;
            cfg.pin_d3 = 14;
            cfg.pin_d4 = 27;
            cfg.pin_d5 = 25;
            cfg.pin_d6 = 33;
            cfg.pin_d7 = 32;

            _bus_instance.config(cfg);              // Apply the bus configuration
            _panel_instance.setBus(&_bus_instance); // Link the panel to the bus
        }

        { // Panel settings configuration
            auto cfg = _panel_instance.config();

            cfg.pin_cs = 17;    // Chip select pin
            cfg.pin_rst = -1;   // Reset pin (-1 if not used)
            cfg.pin_busy = -1;  // Busy pin (-1 if not used)

            cfg.panel_width = 240;          // Actual usable width
            cfg.panel_height = 320;         // Actual usable height
            cfg.offset_x = 0;               // X offset in GRAM
            cfg.offset_y = 0;               // Y offset in GRAM
            cfg.offset_rotation = 0;        // Default rotation offset
            // cfg.dummy_read_pixel = 8;    // (Keep commented unless needed)
            // cfg.dummy_read_bits = 1;     // (Keep commented unless needed)
            cfg.readable = false;           // Panel readable (often false for ST7789)
            cfg.invert = false;             // Invert display colors
            cfg.rgb_order = false;          // false = RGB, true = BGR
            cfg.dlen_16bit = false;         // false = 8bit data length, true = 16bit
            cfg.bus_shared = true;          // Bus shared with other devices (e.g., SD card) - Set appropriately

            _panel_instance.config(cfg); // Apply the panel configuration
        }

        { // Display backlight configuration
            auto cfg = _light_instance.config();

            cfg.pin_bl = 32;            // Pin number to which the backlight is connected
            cfg.invert = false;         // True to invert the brightness of the backlight
            cfg.freq   = 44100;         // Backlight PWM frequency
            cfg.pwm_channel = 7;        // PWM channel number to use

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);   // Set the backlight to the panel.
        }

        setPanel(&_panel_instance); // Set the configured panel as the active panel
    }
};

#endif