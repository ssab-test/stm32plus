/*
 * This file is a part of the open source stm32plus library.
 * Copyright (c) 2011,2012,2013,2014 Andy Brown <www.andybrown.me.uk>
 * Please see website for licensing terms.
 */

#include "config/stm32plus.h"
#include "config/button.h"
#include "config/exti.h"


using namespace stm32plus;


/**
 * Button demo that uses EXTI interrupts to signal that the button is pressed.
 * EXTI allows you to process input from GPIO pins asynchronously.
 *
 * This demo assumes that you have a button on PA8 and an LED on PF6. The LED
 * will light as long as the button is held down.
 *
 * An Exti8 (external interrupt) is attached to PA8 and is configured to trigger
 * on both rising (pressed) and falling (released) edges.
 *
 * To use this demo on the STM32F4DISCOVERY board you will need to make the
 * following changes to target the onboard button and LEDs:
 *
 *  LED_PIN to 13
 *  BUTTON_PIN to 0
 *  GpioF... to GpioD...
 *  Exti8 to Exit0
 *
 * To use this demo on the STM32VLDISCOVERY or the STM32F0DISCOVERY board you
 * will need to make the following changes to target the onboard button and LEDs:
 *
 *  LED_PIN to 8
 *  BUTTON_PIN to 0
 *  GpioF... to GpioC...
 *  Exti8 to Exti0
 *
 * Compatible MCU:
 *   STM32F0
 *   STM32F1
 *   STM32F4
 *
 * Tested on devices:
 *   STM32F042F6P6
 *   STM32F051R8T6
 *   STM32F100RBT6
 *   STM32F103ZET6
 *   STM32F407VGT6
 */

class ExtiTest {

  protected:
    volatile bool _stateChanged;

    enum {
      LED_PIN = 6,
      BUTTON_PIN = 8
    };

  public:
    void run() {

      // initialise the LED and button pins

      GpioF<DefaultDigitalOutputFeature<LED_PIN> > pf;
      GpioA<DefaultDigitalInputFeature<BUTTON_PIN> > pa;

      // enable EXTI on the button pin and subscribe to interrupts

      Exti8 exti(EXTI_Mode_Interrupt,EXTI_Trigger_Rising_Falling,pa[BUTTON_PIN]);

      exti.ExtiInterruptEventSender.insertSubscriber(
          ExtiInterruptEventSourceSlot::bind(this,&ExtiTest::onInterrupt)
        );

      // lights off (this LED is active low, i.e. PF6 is a sink)

      pf[LED_PIN].set();

      // main loop

      for(;;) {

        _stateChanged=false;      // race conditition, but it's demo code...

        // wait for the interrupt to tell us that there's been a button press/release

        while(!_stateChanged);

        // act on the new state and reset for the next run

        pf[LED_PIN].setState(pa[BUTTON_PIN].read());
      }
    }


    /**
     * Interrupt callback from the EXTI interrupt
     */

    void onInterrupt(uint8_t /* extiLine */) {
      _stateChanged=true;
    }
};


/*
 * Main entry point
 */

int main() {

  ExtiTest test;
  test.run();

  // not reached
  return 0;
}
