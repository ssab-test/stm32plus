/*
 * This file is a part of the open source stm32plus library.
 * Copyright (c) 2011,2012,2013,2014 Andy Brown <www.andybrown.me.uk>
 * Please see website for licensing terms.
 */

#include "config/stm32plus.h"
#include "config/display/tft.h"


extern uint32_t BulbPixelsSize,BulbPixels;
extern uint32_t AudioPixelsSize,AudioPixels;
extern uint32_t DocPixelsSize,DocPixels;
extern uint32_t GlobePixelsSize,GlobePixels;
extern uint32_t FlagPixelsSize,FlagPixels;


using namespace stm32plus;
using namespace stm32plus::display;


/**
 * R61523 LCD test specifically for the STM32 VL Discovery board.
 * Show a looping graphics demo. We will make use of the built-in
 * PWM generator to control the backlight. This saves us an MCU
 * output pin and a timer peripheral.
 *
 * It's a 16-bit device and we control it in this demo using the optimised
 * 64K GPIO access mode. To achieve the high speed we access the entire data
 * port (16 bits) in one instruction. You will need to get your soldering
 * iron out and make the following modifications to the VL Discovery board
 * in order to run this demo:
 *
 * Connect solder bridge SB14
 * Connect solder bridge SB15
 * Remove resistor R15 (it's an 0R rating so not really a 'resistor')
 *
 * These modifications make all of GPIO port C available for IO at the
 * expense of losing access to the 32kHz oscillator used to drive the RTC.
 *
 * The wiring that you need to do is as follows:
 *
 * PA0       => RESET
 * PA1       => WR
 * PA2       => RS
 * PC[0..15] => LCD Data [0..15]
 *
 * And for the backlight, connect together the following
 * two pins on the LCD breakout board:
 *
 * BL_PWM => EN
 *
 * Compatible MCU:
 *   STM32F100 VL
 *
 * Tested on devices:
 *   STM32F100RBT6
 */

class R61523Test {

  public:

    // declare the ports and pins that we'll use

    enum {
      Port_DATA    = GPIOC_BASE,
      Port_CONTROL = GPIOA_BASE,

      Pin_RESET    = GPIO_Pin_0,
      Pin_WR       = GPIO_Pin_1,
      Pin_RS       = GPIO_Pin_2
    };

  protected:

    // declare the access mode carefully so that we pick up the optimised implementation

    typedef Gpio16BitAccessMode<R61523Test,COLOURS_16BIT,24,80,80> LcdAccessMode;
    typedef R61523_Landscape_64K<LcdAccessMode> LcdPanel;
    typedef R61523PwmBacklight<LcdAccessMode> LcdBacklight;

    LcdPanel *_gl;
    LcdBacklight *_backlight;
    Font_PROGGYCLEAN16 _font;

  public:
    void run() {

      // declare the access mode

      LcdAccessMode accessMode;

      // declare a panel

      _gl=new LcdPanel(accessMode);

      // apply the gamma curve. Note that gammas are panel specific. This curve is appropriate
      // to a replacement (non-original) panel obtained from ebay.

      uint8_t levels[13]={ 0xe,0,1,1,0,0,0,0,0,0,3,4,0 };
      R61523Gamma gamma(levels);
      _gl->applyGamma(gamma);

      // clear to black while the lights are out

      _gl->setBackground(0);
      _gl->clearScreen();

      // create the backlight using default template parameters

      _backlight=new LcdBacklight(accessMode);

      // fade up the backlight to 100% using the hardware to do the smooth fade

      _backlight->setPercentage(100);

      // A wide range of sample fonts are available. See the "lib/include/display/graphic/fonts"
      // directory for a full list and you can always download and convert your own using the
      // FontConv utility.

      *_gl << _font;

      for(;;) {
        lzgTest();
        basicColoursTest();
        backlightTest();
        gradientTest();
        textTest();
        rectTest();
        lineTest();
        ellipseTest();
        clearTest();
        sleepTest();
      }
    }


    void sleepTest() {

      prompt("Sleep test");

      // go to sleep

      *_gl << Point::Origin << "Sleeping now...";
      MillisecondTimer::delay(1000);
      _gl->sleep();
      MillisecondTimer::delay(3000);

      // wake up

      _gl->wake();
      _gl->clearScreen();
      *_gl << Point::Origin << "Woken up again...";
      MillisecondTimer::delay(3000);
    }


    void lzgTest() {

      prompt("LZG bitmap test");

      drawCompressedBitmap((uint8_t *)&BulbPixels,(uint32_t)&BulbPixelsSize,89,148);
      drawCompressedBitmap((uint8_t *)&AudioPixels,(uint32_t)&AudioPixelsSize,150,161);
      drawCompressedBitmap((uint8_t *)&DocPixels,(uint32_t)&DocPixelsSize,200,240);
      drawCompressedBitmap((uint8_t *)&FlagPixels,(uint32_t)&FlagPixelsSize,144,220);
      drawCompressedBitmap((uint8_t *)&GlobePixels,(uint32_t)&GlobePixelsSize,193,219);
    }


    void drawCompressedBitmap(uint8_t *pixels,uint32_t size,uint16_t width,uint16_t height) {

      _gl->setBackground(ColourNames::WHITE);
      _gl->clearScreen();

      LinearBufferInputOutputStream compressedData(pixels,size);
      LzgDecompressionStream decompressor(compressedData,size);

      _gl->drawBitmap(
          Rectangle((_gl->getWidth()-width)/2,
              (_gl->getHeight()-height)/2,
              width,height),
              decompressor);

      MillisecondTimer::delay(3000);
    }


    void textTest() {

      const char *str="The quick brown fox";
      Size size;
      Point p;
      uint32_t i,start;

      prompt("Stream operators test");

      *_gl << Point::Origin << "Let's see PI:";

      for(i=0;i<=7;i++)
        *_gl << Point(0,(1+i)*_font.getHeight()) << DoublePrecision(3.1415926535,i);

      MillisecondTimer::delay(5000);

      prompt("Opaque text test");

      size=_gl->measureString(_font,str);

      for(i=0,start=MillisecondTimer::millis();MillisecondTimer::millis()-start<5000;i++) {

        p.X=rand() % (_gl->getXmax()-size.Width);
        p.Y=rand() % (_gl->getYmax()-size.Height);

        _gl->setForeground(rand());
        _gl->writeString(p,_font,str);
      }
    }


    void clearTest() {

      int i;
      uint32_t start;

      prompt("Clear screen test");

      for(i=0;i<200;i++) {
        _gl->setBackground(rand());

        start=MillisecondTimer::millis();
        _gl->clearScreen();
        stopTimer(" to clear",MillisecondTimer::millis()-start);
      }
    }


    void basicColoursTest() {

      uint16_t i;

      static const uint32_t colours[8]={
        ColourNames::RED,
        ColourNames::GREEN,
        ColourNames::BLUE,
        ColourNames::CYAN,
        ColourNames::MAGENTA,
        ColourNames::YELLOW,
        ColourNames::BLACK,
        ColourNames::WHITE,
      };

      prompt("Basic colours test");

      for(i=0;i<sizeof(colours)/sizeof(colours[0]);i++) {
        _gl->setBackground(colours[i]);
        _gl->clearScreen();

        MillisecondTimer::delay(500);
      }
    }


    void lineTest() {

      Point p1,p2;
      uint32_t i,start;

      prompt("Line test");

      for(i=0,start=MillisecondTimer::millis();MillisecondTimer::millis()-start<5000;i++) {

        p1.X=rand() % _gl->getXmax();
        p1.Y=rand() % _gl->getYmax();
        p2.X=rand() % _gl->getXmax();
        p2.Y=rand() % _gl->getYmax();

        _gl->setForeground(rand());
        _gl->drawLine(p1,p2);
      }

      _gl->setForeground(ColourNames::WHITE);
      _gl->clearScreen();
      *_gl << Point::Origin << i << " lines in 5 seconds";
      MillisecondTimer::delay(3000);
    }

    void rectTest() {

      uint32_t i,start;
      Rectangle rc;

      prompt("Rectangle test");

      for(i=0,start=MillisecondTimer::millis();MillisecondTimer::millis()-start<5000;i++) {

        rc.X=(rand() % _gl->getXmax()/2);
        rc.Y=(rand() % _gl->getYmax()/2);
        rc.Width=rand() % (_gl->getXmax()-rc.X);
        rc.Height=rand() % (_gl->getYmax()-rc.Y);

        _gl->setForeground(rand());
        _gl->fillRectangle(rc);
      }

      _gl->clearScreen();

      for(i=0,start=MillisecondTimer::millis();MillisecondTimer::millis()-start<5000;i++) {

        rc.X=(rand() % _gl->getXmax()/2);
        rc.Y=(rand() % _gl->getYmax()/2);
        rc.Width=rand() % (_gl->getXmax()-rc.X);
        rc.Height=rand() % (_gl->getYmax()-rc.Y);

        _gl->setForeground(rand());
        _gl->drawRectangle(rc);

        if(i % 1000 ==0)
          _gl->clearScreen();
      }
    }


    void ellipseTest() {

      uint32_t i,start;
      Point p;
      Size s;

      prompt("Ellipse test");
      _gl->setBackground(0);

      for(i=0,start=MillisecondTimer::millis();MillisecondTimer::millis()-start<5000;i++) {

        p.X=_gl->getXmax()/4+(rand() % (_gl->getXmax()/2));
        p.Y=_gl->getYmax()/4+(rand() % (_gl->getYmax()/2));

        if(p.X<_gl->getXmax()/2)
          s.Width=rand() % p.X;
        else
          s.Width=rand() % (_gl->getXmax()-p.X);

        if(p.Y<_gl->getYmax()/2)
          s.Height=rand() % p.Y;
        else
          s.Height=rand() % (_gl->getYmax()-p.Y);

        _gl->setForeground(rand());
        _gl->fillEllipse(p,s);
      }

      _gl->clearScreen();

      for(i=0,start=MillisecondTimer::millis();MillisecondTimer::millis()-start<5000;i++) {

        p.X=_gl->getXmax()/4+(rand() % (_gl->getXmax()/2));
        p.Y=_gl->getYmax()/4+(rand() % (_gl->getYmax()/2));

        if(p.X<_gl->getXmax()/2)
          s.Width=rand() % p.X;
        else
          s.Width=rand() % (_gl->getXmax()-p.X);

        if(p.Y<_gl->getYmax()/2)
          s.Height=rand() % p.Y;
        else
          s.Height=rand() % (_gl->getYmax()-p.Y);

        if(s.Height>0 && s.Width>0 && p.X+s.Width<_gl->getXmax() && p.Y+s.Height<_gl->getYmax()) {
          _gl->setForeground(rand());
          _gl->drawEllipse(p,s);
        }

        if(i % 500==0)
          _gl->clearScreen();
      }
    }

    void doGradientFills(Direction dir) {

      Rectangle rc;
      uint16_t i;
      static uint32_t colours[7]={
        ColourNames::RED,
        ColourNames::GREEN,
        ColourNames::BLUE,
        ColourNames::CYAN,
        ColourNames::MAGENTA,
        ColourNames::YELLOW,
        ColourNames::WHITE,
      };


      rc.Width=_gl->getXmax()+1;
      rc.Height=(_gl->getYmax()+1)/2;

      for(i=0;i<sizeof(colours)/sizeof(colours[0]);i++) {

        rc.X=0;
        rc.Y=0;

        _gl->gradientFillRectangle(rc,dir,ColourNames::BLACK,colours[i]);
        rc.Y=rc.Height;
        _gl->gradientFillRectangle(rc,dir,colours[i],ColourNames::BLACK);

        MillisecondTimer::delay(1000);
      }
    }

    void gradientTest() {

      prompt("Gradient test");

      doGradientFills(HORIZONTAL);
      doGradientFills(VERTICAL);
    }


    void backlightTest() {

      prompt("Backlight test");

      Rectangle rc;
      uint16_t i;
      static uint32_t colours[8]={
        ColourNames::RED,
        ColourNames::GREEN,
        ColourNames::BLUE,
        ColourNames::CYAN,
        ColourNames::MAGENTA,
        ColourNames::YELLOW,
        ColourNames::WHITE,
        ColourNames::BLACK,
      };

      // draw a row of solid colours

      rc.X=0;
      rc.Y=0;
      rc.Height=_gl->getHeight()/2;
      rc.Width=_gl->getWidth()/(sizeof(colours)/sizeof(colours[0]));

      for(i=0;i<sizeof(colours)/sizeof(colours[0]);i++) {

        _gl->setForeground(colours[i]);
        _gl->fillRectangle(rc);

        rc.X+=rc.Width;
      }

      // draw a greyscale

      rc.X=0;
      rc.Y=rc.Height;
      rc.Height=rc.Height/4;
      rc.Width=_gl->getWidth()/256;

      for(i=0;i<256;i++) {
        _gl->setForeground(i | (i << 8) | (i << 16));
        _gl->fillRectangle(rc);
        rc.X+=rc.Width;
      }

      for(i=100;i>0;i-=5) {

        // set the level

        _backlight->setPercentage(i);

        // show the indicator

        rc.X=_gl->getWidth()/4;
        rc.Y=(_gl->getHeight()*6)/8;
        rc.Height=_gl->getHeight()/8;

        // fill

        rc.Width=(_gl->getWidth()/2*i)/100;
        _gl->gradientFillRectangle(rc,Direction::HORIZONTAL,0x008000,0x00ff00);

        // remainder

        rc.X+=rc.Width;
        rc.Width=_gl->getWidth()/2-rc.Width;
        _gl->setForeground(ColourNames::BLACK);
        _gl->fillRectangle(rc);

        // show the percentage

        _gl->setForeground(ColourNames::WHITE);
        *_gl << Point(0,_gl->getHeight()-_font.getHeight()) << "Backlight level: " << i << "%  ";

        // pause

        MillisecondTimer::delay(750);
      }

      // restore backlight

      _backlight->setPercentage(100);
    }


    void prompt(const char *prompt) {

      _gl->setBackground(ColourNames::BLACK);
      _gl->clearScreen();

      _gl->setForeground(ColourNames::WHITE);
      *_gl << Point(0,0) << prompt;

      MillisecondTimer::delay(2000);
      _gl->clearScreen();
    }


    void stopTimer(const char *prompt,uint32_t elapsed) {

      _gl->setForeground(0xffffff);
      *_gl << Point(0,0) << (int32_t)elapsed << "ms " << prompt;
    }
};


/*
 * Main entry point
 */

int main() {

  // set up SysTick at 1ms resolution
  MillisecondTimer::initialise();

  R61523Test test;
  test.run();

  // not reached
  return 0;
}
