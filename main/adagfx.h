#ifndef _ADAFRUIT_GFX_H
#define _ADAFRUIT_GFX_H

#include <cstdint>
#include <memory>
#include <string>

#include "gfxfont.h"

class Adafruit_GFX {
   public:
    Adafruit_GFX();  // Constructor

    const uint8_t *getBuffer();

    inline void drawPixel(int16_t x, int16_t y, uint8_t color) {
        if (x < 0 || y < 0) return;

        const size_t pixel = (_width * y + x);
        if (pixel >= _width * _height) return;

        const size_t index = pixel >> 3;
        const uint8_t shift = pixel & 0x07;

        if (color)
            buffer.get()[index] &= ~(0x80 >> shift);
        else
            buffer.get()[index] |= (0x80 >> shift);
    }

    void drawLineGeneric(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);

    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
    void fillScreen(uint8_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

    // These exist only with Adafruit_GFX (no subclass overrides)
    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color);
    void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint8_t color);
    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color);
    void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint8_t color);
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);
    void drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint8_t color);
    void fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint8_t color);
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color);
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color, uint8_t bg);
    void drawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color);
    void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size);
    void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size_x, uint8_t size_y);
    void getTextBounds(const char *string, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);
    void getTextBounds(const std::string &str, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w,
                       uint16_t *h);
    void setTextSize(uint8_t s);
    void setTextSize(uint8_t sx, uint8_t sy);
    void setFont(const GFXfont *f = NULL);

    void write(char c);
    void write(const char *s);
    void write(const std::string &s);

    template <typename T>
    void write(int16_t x, int16_t y, T s) {
        setCursor(x, y);
        write(s);
    }

    void setCursor(int16_t x, int16_t y) {
        cursor_x = x;
        cursor_y = y;
    }

    void setTextColor(uint16_t c) { textcolor = textbgcolor = c; }

    void setTextColor(uint16_t c, uint16_t bg) {
        textcolor = c;
        textbgcolor = bg;
    }

    void setTextWrap(bool w) { wrap = w; }

    void cp437(bool x = true) { _cp437 = x; }

    int16_t width(void) const { return _width; };
    int16_t height(void) const { return _height; }

    int16_t getCursorX(void) const { return cursor_x; }
    int16_t getCursorY(void) const { return cursor_y; };

   private:
    void charBounds(unsigned char c, int16_t *x, int16_t *y, int16_t *minx, int16_t *miny, int16_t *maxx,
                    int16_t *maxy);

   private:
    static constexpr int16_t _width = 400;   ///< Display width as modified by current rotation
    static constexpr int16_t _height = 300;  ///< Display height as modified by current rotation

    std::unique_ptr<uint8_t[]> buffer;

    int16_t cursor_x;      ///< x location to start print()ing text
    int16_t cursor_y;      ///< y location to start print()ing text
    uint16_t textcolor;    ///< 16-bit background color for print()
    uint16_t textbgcolor;  ///< 16-bit text color for print()
    uint8_t textsize_x;    ///< Desired magnification in X-axis of text to print()
    uint8_t textsize_y;    ///< Desired magnification in Y-axis of text to print()
    uint8_t rotation;      ///< Display rotation (0 thru 3)
    bool wrap;             ///< If set, 'wrap' text at right edge of display
    bool _cp437;           ///< If set, use correct CP437 charset (default is off)
    GFXfont *gfxFont;      ///< Pointer to special font
};

#endif  // _ADAFRUIT_GFX_H
