/**
 *  \file   main.c
 *  \brief  eZ430-RF2500 tutorial, serial
 *  \author Antoine Fraboulet, Tanguy Risset
 *  \date   2010
 **/

#include <msp430x22x4.h>
#include <msp430f2274.h>
#if defined(__GNUC__) && defined(__MSP430__)
/* This is the MSPGCC compiler */
#include <msp430.h>
#include <iomacros.h>
#include <legacymsp430.h>
#elif defined(__IAR_SYSTEMS_ICC__)
/* This is the IAR compiler */
//#include <io430.h>
#endif

#include <stdio.h>
#include <string.h>

#include "leds.h"
#include "clock.h"
#include "watchdog.h"
#include "uart.h"
#include "lpm_compat.h"

#include "math.h"
#include "timer.h"
#include "spi.h"
#include "cc2500.h"

#define MSG_SIZE 32
#define USE_CONFIGURATION_0
#define ID 1

#if defined(USE_CONFIGURATION_0)
#warning "** **************************************************"
#warning "** Using user configuration 0"
#warning "** **************************************************"
#define USER_RFCONFIG config0

const RF_SETTINGS config0 = {
    0x12,   // FSCTRL1   Frequency synthesizer control.
    0x00,   // FSCTRL0   Frequency synthesizer control.
    0x5D,   // FREQ2     Frequency control word, high byte.
    0x93,   // FREQ1     Frequency control word, middle byte.
    0xB1,   // FREQ0     Frequency control word, low byte.
    0x2D,   // MDMCFG4   Modem configuration.
    0x3B,   // MDMCFG3   Modem configuration.
    0xF3,   // MDMCFG2   Modem configuration.
    0x22,   // MDMCFG1   Modem configuration.
    0xF8,   // MDMCFG0   Modem configuration.
    0x05,   // CHANNR    Channel number.
    0x01,   // DEVIATN   Modem deviation setting (when FSK modulation is enabled).
    0xB6,   // FREND1    Front end RX configuration.
    0x10,   // FREND0    Front end TX configuration.
    0x18,   // MCSM0     Main Radio Control State Machine configuration.
    0x1D,   // FOCCFG    Frequency Offset Compensation Configuration.
    0x1C,   // BSCFG     Bit synchronization Configuration.
    0xC7,   // AGCCTRL2  AGC control.
    0x00,   // AGCCTRL1  AGC control.
    0xB0,   // AGCCTRL0  AGC control.
    0xEA,   // FSCAL3    Frequency synthesizer calibration.
    0x0A,   // FSCAL2    Frequency synthesizer calibration.
    0x00,   // FSCAL1    Frequency synthesizer calibration.
    0x11,   // FSCAL0    Frequency synthesizer calibration.
    0x59,   // FSTEST    Frequency synthesizer calibration.
    0x88,   // TEST2     Various test settings.
    0x31,   // TEST1     Various test settings.
    0x0B,   // TEST0     Various test settings.
    0x07,   // FIFOTHR   RXFIFO and TXFIFO thresholds.
    0x29,   // IOCFG2    GDO2 output pin configuration.
    0x06,   // IOCFG0D   GDO0 output pin configuration. Refer to SmartRF® Studio User Manual for detailed pseudo register explanation.
    0x04,   // PKTCTRL1  Packet automation control.
    0x05,   // PKTCTRL0  Packet automation control.
    0x00,   // ADDR      Device address.
    0xFF    // PKTLEN    Packet length.
};
#endif

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

#define SERIAL_RX_FIFO_SIZE 8
#define CMD_SIZE 32

volatile uint8_t serial_rx_buffer[SERIAL_RX_FIFO_SIZE];
volatile uint8_t serial_rx_rptr;
volatile uint8_t serial_rx_wptr;
volatile uint8_t serial_rx_size;

/* ************************************************** */
#define ID 3

void tx(char cmd[])
{
  int i; 
  cc2500_utx(cmd,strlen(cmd)+1);
  printf("\ntx send msg : %s \n",cmd);
}

void serial_ring_init()
{
  serial_rx_rptr = 0;
  serial_rx_wptr = 0;
  serial_rx_size = 0;
}

void serial_ring_put(uint8_t data)
{
  serial_rx_buffer[serial_rx_wptr] = data;
  serial_rx_wptr = (serial_rx_wptr + 1) % SERIAL_RX_FIFO_SIZE;
  if (serial_rx_size < SERIAL_RX_FIFO_SIZE)
  {
    serial_rx_size ++;
  }
  else
  {
    /*
     * if (serial_rx_size == SERIAL_RX_FIFO_SIZE)
     * we get a rx_overflow
     */
  }
}

int serial_ring_get(uint8_t *data)
{
  int ret = 0;
  dint();
  if (serial_rx_size > 0)
  {
    *data = serial_rx_buffer[serial_rx_rptr];
    serial_rx_rptr = (serial_rx_rptr + 1) % SERIAL_RX_FIFO_SIZE;
    serial_rx_size --;
    ret = 1;
  }
  eint();
  return ret;
}

/* ************************************************** */
/* ************************************************** */
/* ************************************************** */

int serial_cb(unsigned char data)
{
  serial_ring_put(data);
  return 1; /* will wakeup from LPMx */
}

void execcmd(char cmd[]){

  if(strcmp(cmd, "test") == 0){
    printf("\nHello World");
  }
  else if(strcmp(cmd, "help") == 0){
    printf("\ntest   : Hello World\ngreen led on : Turn green led on\ngreen led off : Turn green led off\n");
  }
  else if(strcmp(cmd, "green led on") == 0){
    led_green_on();
  }
  else if(strcmp(cmd, "green led off") == 0){
    led_green_off();
  }
  else{
    printf("\nCommande inconnue : help for help");
  }
  tx(cmd);
  printf("\n");
}

int main(void)
{
  uint8_t data;
  char cmd[CMD_SIZE];
  int i=0;
  int j;
  watchdog_stop();
  set_mcu_speed_dco_mclk_16MHz_smclk_8MHz();
  leds_init();
  led_red_on();

  //Init Serial
  uart_init(UART_9600_SMCLK_8MHZ);
  serial_ring_init();
  uart_register_cb( serial_cb);

  //Init radio
  spi_init();
  cc2500_init();
#if defined(USER_RFCONFIG)
  cc2500_configure(& USER_RFCONFIG );
#endif

  printf("serial test application: echo\n\r");
  led_green_on();
  eint();

  for(;;)
  {
    LPM(1);
    if (serial_ring_get(&data))
    {
      if(data != '\r' && i<CMD_SIZE-1){
        putchar(data);
        cmd[i]=data;
        i++;
      }
      else{
        cmd[i]='\0';
        execcmd(&cmd);
        // Reset cmd
        for(j=0;j<CMD_SIZE;j++){
          cmd[j]=NULL;
        }
        i=0;
      }
    }
    else
    {
      printf("\n\n serial_ring_get() returns 0 : empty ring\n\n");
      led_red_switch();
    }
  }
}

/* ************************************************** */
/* ************************************************** */
