/*
 * This file is a part of the open source stm32plus library.
 * Copyright (c) 2011,2012,2013,2014 Andy Brown <www.andybrown.me.uk>
 * Please see website for licensing terms.
 */

#include "config/stm32plus.h"
#include "config/net.h"
#include "config/timing.h"
#include "config/usart.h"
#include "config/string.h"


using namespace stm32plus;
using namespace stm32plus::net;


/**
 * This example demonstrates the ICMP transport by sending periodic echo requests (pings) to a
 * hardcoded IP address (change it to suit your network).
 *
 * This network stack is about as simple as it gets. We don't even use DHCP for client configuration
 * so you'll need to be connected to a network that understands that you have the static IP address
 * configured in this example.
 *
 * Here's how the network stack for this example is configured:
 *
 *              +----------------+-----------+
 * APPLICATION: | StaticIpClient | Ping      |
 *              +----------------------------+
 * TRANSPORT:   | Icmp                       |
 *              +-----+----------------------+
 * NETWORK      | DefaultIp | Arp            |
 *              +-----+----------------+-----+
 * DATALINK:    | DefaultRmiiInterface | Mac |
 *              +----------------------+-----+
 * PHYSICAL:    | DP83848C                   |
 *              +-----------------------------
 *
 * This example is also tested using the KSZ8051MLL in MII mode instead of the DP83848C/RMII. The
 * KSZ8051MLL test was performed on the STM32F107. The DP83848C was tested on the STM32F407. To
 * reconfigure this demo for the F107 using remapped MAC pins connected to the KSZ8051MLL change the
 * physical and datalink layers thus:
 *
 * typedef PhysicalLayer<KSZ8051MLL> MyPhysicalLayer;
 * typedef DatalinkLayer<MyPhysicalLayer,RemapMiiInterface,Mac> MyDatalinkLayer;
 *
 * Tested on devices:
 *   STM32F107VCT6
 *   STM32F407VGT6
 */

class NetPingClientTest {

  public:

    /**
     * Types that define the network stack
     */

    typedef PhysicalLayer<DP83848C> MyPhysicalLayer;
    typedef DatalinkLayer<MyPhysicalLayer,DefaultRmiiInterface,Mac> MyDatalinkLayer;
    typedef NetworkLayer<MyDatalinkLayer,Arp,DefaultIp> MyNetworkLayer;
    typedef TransportLayer<MyNetworkLayer,Icmp> MyTransportLayer;
    typedef ApplicationLayer<MyTransportLayer,StaticIpClient,Ping> MyApplicationLayer;
    typedef NetworkStack<MyApplicationLayer> MyNetworkStack;


    /*
     * Set by the PHY IRQ when there has been a change to the link status
     */

    volatile bool _linkStatusChanged;


    /*
     * The network stack object
     */

    MyNetworkStack *_net;


    /*
     * Declare the USART that we'll use. On my dev board USART3 is mapped to PC10,11
     * and we have that defined as remap #2
     */

    typedef Usart3_Remap2<> MyUsart;
    MyUsart *_usart;
    UsartPollingOutputStream *_outputStream;

    /*
     * Run the test
     */

    void run() {

      // reset state

      _linkStatusChanged=false;

      // declare an instance of the USART and the stream that we'll use to write to it

      _usart=new MyUsart(57600);
      _outputStream=new UsartPollingOutputStream(*_usart);

      // declare the RTC that that stack requires. it's used for cache timeouts, DHCP lease expiry
      // and such like so it does not have to be calibrated for accuracy. A few seconds here or there
      // over a 24 hour period isn't going to make any difference.

      Rtc<RtcLsiClockFeature<Rtc32kHzLsiFrequencyProvider>,RtcSecondInterruptFeature> rtc;
      rtc.setTick(0);

      // declare an instance of the network stack

      MyNetworkStack::Parameters params;
      _net=new MyNetworkStack;

      params.base_rtc=&rtc;

      // declare our IP address and subnet mask

      params.staticip_address="192.168.0.10";
      params.staticip_subnetMask="255.255.255.0";
      params.staticip_defaultGateway="192.168.0.1";

      // Initialise the stack. This will reset the PHY, initialise the MAC
      // and attempt to create a link to our link partner. Ensure your cable
      // is plugged in when you run this or be prepared to handle the error

      if(!_net->initialise(params))
        error();

      // we'd like to be notified when there's a change in the link status so configure the
      // PHY interrupt mask to report that change. My development board has the PHY interrupt
      // line on PB14 so we'll need an active-low EXTI configured for that

      GpioB<DefaultDigitalInputFeature<14> > pb;
      Exti14 exti(EXTI_Mode_Interrupt,EXTI_Trigger_Falling,pb[14]);

      exti.ExtiInterruptEventSender.insertSubscriber(
          ExtiInterruptEventSourceSlot::bind(this,&NetPingClientTest::onLinkStatusChange)
        );

      if(!_net->phyEnableInterrupts(DP83848C::INTERRUPT_LINK_STATUS_CHANGE))
        error();

      // subscribe to error events from the network stack

      _net->NetworkErrorEventSender.insertSubscriber(NetworkErrorEventSourceSlot::bind(this,&NetPingClientTest::onError));

      // start the ethernet MAC Tx/Rx DMA channels

      if(!_net->startup())
        error();

      for(;;) {

        char buf[20];
        uint32_t elapsed;

        // send a ping every 2 seconds

        if(_net->ping("192.168.1.2",elapsed)) {
          StringUtil::modp_uitoa10(elapsed,buf);
          *_outputStream << "Reply received in " << buf << "ms.\r\n";
        }
        else
          *_outputStream << "Timed out waiting for a reply\r\n";

        MillisecondTimer::delay(1000);

        // check on the link state

        if(_linkStatusChanged) {
          *_outputStream << "The link state changed\r\n";
          _linkStatusChanged=false;
        }
      }
    }


    /**
     * Network error event received, report it
     * @param ned
     */

    void onError(NetEventDescriptor& ned) {

      NetworkErrorEvent& errorEvent(static_cast<NetworkErrorEvent&>(ned));

      char buf[20];

      *_outputStream << "Error (provider/code/cause) ";

      StringUtil::modp_uitoa10(errorEvent.provider,buf);
      *_outputStream << buf;

      StringUtil::modp_uitoa10(errorEvent.code,buf);
      *_outputStream << "/" << buf;

      StringUtil::modp_uitoa10(errorEvent.cause,buf);
      *_outputStream << "/" << buf << "\r\n";
    }


    /**
     * Interrupt callback from the EXTI interrupt. Set the flag that main loop will act on.
     */

    void onLinkStatusChange(uint8_t /* extiLine */) {
      _linkStatusChanged=true;
      _net->phyClearPendingInterrupts();
    }


    void error() {
      *_outputStream << "Aborted execution due to an unexpected error\r\n";
      for(;;);
    }
};


/*
 * Main entry point
 */

int main() {

  // interrupts
  Nvic::initialise();

  // set up SysTick at 1ms resolution
  MillisecondTimer::initialise();

  NetPingClientTest test;
  test.run();

  // not reached
  return 0;
}
