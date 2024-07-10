/***************************************************************************
 *   Copyright (C) 2021 by Salaorni Davide, Velati Matteo                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#pragma once

#include <config/mxgui_settings.h>
#include "display.h"
#include "point.h"
#include "color.h"
#include "iterator_direction.h"
#include "miosix.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace mxgui {

//The ST7735 driver requires a color depth 16 per pixel
#ifdef MXGUI_COLOR_DEPTH_16_BIT

//Used to transiently pull low either the csx or dcx pin
class Transaction
{
public:
    Transaction(miosix::GpioPin pin) : pin(pin)  { pin.low();  }
    ~Transaction() { pin.high(); }
private:
    miosix::GpioPin pin;
};

/**
 * Generic driver for a ST7735 display. The SPI interface and mapping of the
 * csx, dcx and resx pins is retargetable.
 */
class DisplayGenericST7735 : public Display
{
public:
    /**
     * Turn the display On after it has been turned Off.
     * Display initial state On.
     */
    void doTurnOn() override;

    /**
     * Turn the display Off. It can be later turned back On.
     */
    void doTurnOff() override;

    /**
     * Set display brightness. Depending on the underlying driver,
     * may do nothing.
     * \param brt from 0 to 100
     */
    void doSetBrightness(int brt) override;

    /**
     * \return a pair with the display height and width
     */
    std::pair<short int, short int> doGetSize() const override;

    /**
     * Write text to the display. If text is too long it will be truncated
     * \param p point where the upper left corner of the text will be printed
     * \param text, text to print.
     */
    void write(Point p, const char *text) override;

    /**
     * Write part of text to the display
     * \param p point of the upper left corner where the text will be drawn.
     * Negative coordinates are allowed, as long as the clipped view has
     * positive or zero coordinates
     * \param a Upper left corner of clipping rectangle
     * \param b Lower right corner of clipping rectangle
     * \param text text to write
     */
    void clippedWrite(Point p, Point a,  Point b, const char *text) override;

    /**
     * Clear the Display. The screen will be filled with the desired color
     * \param color fill color
     */
    void clear(Color color) override;

    /**
     * Clear an area of the screen
     * \param p1 upper left corner of area to clear
     * \param p2 lower right corner of area to clear
     * \param color fill color
     */
    void clear(Point p1, Point p2, Color color) override;

    /**
     * This member function is used on some target displays to reset the
     * drawing window to its default value. You have to call beginPixel() once
     * before calling setPixel(). You can then make any number of calls to
     * setPixel() without calling beginPixel() again, as long as you don't
     * call any other member function in this class. If you call another
     * member function, for example line(), you have to call beginPixel() again
     * before calling setPixel().
     */
    void beginPixel() override;

    /**
     * Draw a pixel with desired color. 
     * \param p point where to draw pixel
     * \param color pixel color
     */
    void setPixel(Point p, Color color) override;

    /**
     * Draw a line between point a and point b, with color c
     * \param a first point
     * \param b second point
     * \param c line color
     */
    void line(Point a, Point b, Color color) override;

    /**
     * Draw an horizontal line on screen.
     * Instead of line(), this member function takes an array of colors to be
     * able to individually set pixel colors of a line.
     * \param p starting point of the line
     * \param colors an array of pixel colors whoase size must be b.x()-a.x()+1
     * \param length length of colors array.
     * p.x()+length must be <= display.width()
     */
    void scanLine(Point p, const Color *colors, unsigned short length) override;

    /**
     * \return a buffer of length equal to this->getWidth() that can be used to
     * render a scanline.
     */
    Color *getScanLineBuffer() override;

    /**
     * Draw the content of the last getScanLineBuffer() on an horizontal line
     * on the screen.
     * \param p starting point of the line
     * \param length length of colors array.
     * p.x()+length must be <= display.width()
     */
    void scanLineBuffer(Point p, unsigned short length) override;

    /**
     * Draw an image on the screen
     * \param p point of the upper left corner where the image will be drawn
     * \param i image to draw
     */
    void drawImage(Point p, const ImageBase& img) override;

    /**
     * Draw part of an image on the screen
     * \param p point of the upper left corner where the image will be drawn.
     * Negative coordinates are allowed, as long as the clipped view has
     * positive or zero coordinates
     * \param a Upper left corner of clipping rectangle
     * \param b Lower right corner of clipping rectangle
     * \param img Image to draw
     */
    void clippedDrawImage(Point p, Point a, Point b, const ImageBase& img) override;

     /**
     * Draw a rectangle (not filled) with the desired color
     * \param a upper left corner of the rectangle
     * \param b lower right corner of the rectangle
     * \param c color of the line
     */
    void drawRectangle(Point a, Point b, Color c) override;

     /**
     * Make all changes done to the display since the last call to update()
     * visible.
     */
    void update() override;

    /**
     * Pixel iterator. A pixel iterator is an output iterator that allows to
     * define a window on the display and write to its pixels.
     */
    class pixel_iterator
    {
    public:
        /**
         * Default constructor, results in an invalid iterator.
         */
        pixel_iterator(): pixelLeft(0) {}

        /**
         * Set a pixel and move the pointer to the next one
         * \param color color to set the current pixel
         * \return a reference to this
         */
        pixel_iterator& operator= (Color color)
        {
            pixelLeft--;

            unsigned char lsb = color & 0xFF;
            unsigned char msb = (color >> 8) & 0xFF;

            Transaction t(display->csx);
            display->writeRam(msb);
            display->writeRam(lsb);

            return *this;
        }

        /**
         * Compare two pixel_iterators for equality.
         * They are equal if they point to the same location.
         */
        bool operator== (const pixel_iterator& itr)
        {
            return this->pixelLeft==itr.pixelLeft;
        }

        /**
         * Compare two pixel_iterators for inequality.
         * They different if they point to different locations.
         */
        bool operator!= (const pixel_iterator& itr)
        {
            return this->pixelLeft!=itr.pixelLeft;
        }

        /**
         * \return a reference to this.
         */
        pixel_iterator& operator* () { return *this; }

        /**
         * \return a reference to this. Does not increment pixel pointer.
         */
        pixel_iterator& operator++ () { return *this; }

        /**
         * \return a reference to this. Does not increment pixel pointer.
         */
        pixel_iterator& operator++ (int) { return *this; }

        /**
         * Must be called if not all pixels of the required window are going
         * to be written.
         */
        void invalidate() {}

    private:
        /**
         * Constructor
         * \param pixelLeft number of remaining pixels
         */
        pixel_iterator(unsigned int pixelLeft): pixelLeft(pixelLeft) {}

        unsigned int pixelLeft; ///< How many pixels are left to draw
        DisplayGenericST7735 *display;

        friend class DisplayGenericST7735; //Needs access to ctor
    };

    /**
     * Specify a window on screen and return an object that allows to write
     * its pixels.
     * Note: a call to begin() will invalidate any previous iterator.
     * \param p1 upper left corner of window
     * \param p2 lower right corner (included)
     * \param d increment direction
     * \return a pixel iterator
     */
    pixel_iterator begin(Point p1, Point p2, IteratorDirection d);

    /**
     * \return an iterator which is one past the last pixel in the pixel
     * specified by begin. Behaviour is undefined if called before calling
     * begin()
     */
    pixel_iterator end() const
    {
        // Default ctor: pixelLeft is zero
        return pixel_iterator();
    }

    /**
     * Destructor
     */
    ~DisplayGenericST7735() override;

protected:

    /**
     * Constructor.
     * \param csx chip select pin
     * \param dcx data/command pin
     * \param resx reset pin
     */
    DisplayGenericST7735(miosix::GpioPin csx,
                         miosix::GpioPin dcx,
                         miosix::GpioPin resx);

    void initialize();
    
    miosix::GpioPin csx;    ///< Chip select
    miosix::GpioPin dcx;    ///< Data/Command
    miosix::GpioPin resx;   ///< Reset
    
private:

    #ifdef MXGUI_ORIENTATION_VERTICAL
    static const short int width    = 128;
    static const short int height   = 160;
    #else //MXGUI_ORIENTATION_HORIZONTAL
    static const short int width    = 160;
    static const short int height   = 128;
    #endif

    /**
     * Set cursor to desired location
     * \param point where to set cursor (0<=x<=127, 0<=y<=159)
     */
    inline void setCursor(Point p)
    {
        window(p, p, false);
    }

    /**
     *  Register 0x36: MADCTL 
     *       bit 7------0
     *        4: |||||+--  MH horizontal referesh (0 L-to-R, 1 R-to-L)
     *        8: ||||+---  RGB BRG order (0 for RGB)
     *       16: |||+----  ML vertical refesh (0 T-to-B, 1 B-to-T)
     *       32: ||+-----  MV row column exchange (1 for X-Y exchange)
     *       64: |+------  MX column address order (1 for mirror X axis)
     *      128: +-------  MY row address order (1 for mirror Y axis)
     */

    /**
     * Set a hardware window on the screen, optimized for writing text.
     * The GRAM increment will be set to up-to-down first, then left-to-right
     * which is the correct increment to draw fonts
     * \param p1 upper left corner of the window
     * \param p2 lower right corner of the window
     */
    inline void textWindow(Point p1, Point p2)
    {
        #ifdef MXGUI_ORIENTATION_VERTICAL
            writeReg (0x36, 0xE0);      // MADCTL:  MX + MY + MV
            window(p1, p2, true);
        #else //MXGUI_ORIENTATION_HORIZONTAL
            writeReg (0x36, 0x80);      // MADCTL:  MY
            window(p1, p2, true);
        #endif
    }

    /**
     * Set a hardware window on the screen, optimized for drawing images.
     * The GRAM increment will be set to left-to-right first, then up-to-down
     * which is the correct increment to draw images
     * \param p1 upper left corner of the window
     * \param p2 lower right corner of the window
     */
    inline void imageWindow(Point p1, Point p2)
    {
        #ifdef MXGUI_ORIENTATION_VERTICAL
            writeReg (0x36, 0xC0);      // MADCTL:  MX + MY
            window(p1, p2, false);
        #else //MXGUI_ORIENTATION_HORIZONTAL
            writeReg (0x36, 0xA0);      // MADCTL:  MY + MV
            window(p1, p2, false);
        #endif
    }

    /**
     * Common part of all window commands
     */
    void window(Point p1, Point p2, bool swap);

    /**
     * Sends command 0x2C to signal the start of data sending
     */
    void writeRamBegin()
    {
        Transaction c(dcx);
        writeRam(0x2C);     //ST7735_RAMWR, to write the GRAM
    }

    /**
     * Used to send pixel data to the display's RAM, and also to send commands.
     * The SPI chip select must be low before calling this member function
     * \param data data to write
     */
    virtual unsigned char writeRam(unsigned char data) = 0;

    /**
     * Write data to a display register
     * \param reg which register?
     * \param data data to write
     */
    virtual void writeReg(unsigned char reg, unsigned char data) = 0;

    /**
     * Write data to a display register
     * \param reg which register?
     * \param data data to write, if null only reg will be written (zero arg cmd)
     * \param len length of data, number of argument bytes
     */
    virtual void writeReg(unsigned char reg, const unsigned char *data=0, int len=1) = 0;

    /**
     * Send multiple commands to the display MCU (we use to send init sequence)
     * \param cmds static array containing the commands
     */
    void sendCmds(const unsigned char *cmds);

    Color *buffer;          ///< For scanLineBuffer
};

#endif //MXGUI_COLOR_DEPTH_16_BIT

} //namespace mxgui
