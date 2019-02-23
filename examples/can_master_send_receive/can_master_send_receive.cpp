/*
 * This file is a part of the open source stm32plus library.
 * Copyright (c) 2011,2012,2013 Andy Brown <www.andybrown.me.uk>
 * Please see website for licensing terms.
 */

#include "config/stm32plus.h"
#include "config/timing.h"
#include "config/can.h"


using namespace stm32plus;


/**
 * This example initialize the CAN peripheral at 500 kBits/s with 87.5% sampling point.
 * To get incoming messages needed to bypass the CAN filtering.
 * With the bypassing we get all messages to FIFO 0, so we need to enable the FMP0
 * interrupt.
 *
 * After the peripheral is initialised we go into an infinite loop sending and receiving
 * 8-byte frames and checking the data content after each reception. If it works then
 * a LED on PF6 will be flashed at 1Hz. If something goes wrong then the PF6 will be set
 * high and the firmware will lock up.
 *
 * If your board does not have a LED on PF6 then you will need to adjust LED_PIN and the
 * GpioF declarations accordingly.
 *
 * Compatible MCU:
 *   STM32F1
 *   STM32F4
 *
 * Tested on devices:
 *   STM32F103C8T6
 *   STM32F103ZET6
 *   STM32F407VGT6
 */


class CanMasterSendReceive {

  private:

    // declare the CAN1 master instance

    Can1<
      Can1InterruptFeature,           // interrupt driven reception
      CanLoopbackModeFeature,         // running in loopback mode
      Can1FilterBypassFeature
    > _can;

    volatile bool _ready;
    volatile uint8_t _receiveData[8];

    enum { LED_PIN = 6 };

  public:

    CanMasterSendReceive() : _can( { 500000,875 } ) {
    }

    void run() {

      uint32_t now;
      bool ledState;
      uint8_t nextByte,i;
      uint8_t sendData[8];

      /*
       * subscribe to receive interrupts and enable FMP0
       */

      _can.CanInterruptEventSender.insertSubscriber(CanInterruptEventSourceSlot::bind(this,&CanMasterSendReceive::onCanInterrupt));
      _can.enableInterrupts(CAN_IT_FMP0);

      /*
       * set up the LED on PF6
       */

      GpioF<DefaultDigitalOutputFeature<LED_PIN>> pf;

      /*
       * Go into an infinite loop sending a message per second
       */

      nextByte=0;
      ledState=false;

      for(;;) {

        // create an 8-byte message

        for(i=0;i<sizeof(sendData);sendData[i++]=nextByte++);

        // prepare the receive state

        _ready=false;
        for(i=0;i<sizeof(_receiveData);_receiveData[i++]='\0');

        // send the message

        _can.send(0x100,sizeof(sendData),sendData);

        // wait for it to arrive for a maximum of 5 seconds

        now=MillisecondTimer::millis();

        while(!_ready)
          if(MillisecondTimer::hasTimedOut(now,5000))
            error(pf[LED_PIN]);

        // check the data content

        for(i=0;i<sizeof(sendData);i++)
          if(sendData[i]!=_receiveData[i])
            error(pf[LED_PIN]);

        // wait for a second and toggle the LED to indicate we're working

        ledState^=true;
        pf[LED_PIN].setState(ledState);

        MillisecondTimer::delay(1000);
      }
    }


    /*
     * receive IRQ callback
     */

    void onCanInterrupt(CanEventType cet) {

    	CanRxMsg msg;
    	uint8_t i;

    	if(cet == CanEventType::EVENT_FIFO0_MESSAGE_PENDING) {

    	  _can.receive(CAN_FIFO0, &msg);

    	  for(i=0;i<sizeof(_receiveData);i++)
    	    _receiveData[i]=msg.Data[i];

    	  _ready=true;
      }
    }


    /*
     * An error occurred, lock up with the LED on
     */

    void error(const GpioPinRef& led) {
      led.set();
      for(;;);
    }
};


int main() {

  // we're using interrupts, initialise NVIC
  Nvic::initialise();

  // set up SysTick at 1ms resolution
  MillisecondTimer::initialise();

  CanMasterSendReceive test;
  test.run();

  // not reached
  return 0;
}





