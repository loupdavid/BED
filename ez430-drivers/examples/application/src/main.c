/**
 *  \file   main.c
 *  \brief  eZ430-RF2500 tutorial, serial
 *  \author Antoine Fraboulet, Tanguy Risset
 *  \date   2010
 **/

#include <msp430x22x4.h>

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

#define ID 1

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
    printf("\n\rHello World");
  }
  else if(strcmp(cmd, "help") == 0){
    printf("\n\rtest   : Hello World\n\rgreen led on : Turn green led on\n\rgreen led off : Turn green led off\n\r");
  }
  else if(strcmp(cmd, "green led on") == 0){
    led_green_on();
  }
  else if(strcmp(cmd, "green led off") == 0){
    led_green_off();
  }
  else{
    printf("\n\rCommande inconnue : help for help");
  }
  printf("\n\r");
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

  uart_init(UART_9600_SMCLK_8MHZ);
  serial_ring_init();
  uart_register_cb( serial_cb);

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
