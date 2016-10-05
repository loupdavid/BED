/**
 *  \file   main.c
 *  \brief  eZ430-RF2500 tutorial, protothread
 *  \author Antoine Fraboulet, Tanguy Risset, Sebastien Mazy
 *  \date   2009
 **/

#include <msp430x22x4.h>

#if defined(__GNUC__) && defined(__MSP430__)
/* This is the MSPGCC compiler */
#include <io.h>
#include <iomacros.h>
#include <signal.h>
#elif defined(__IAR_SYSTEMS_ICC__)
/* This is the IAR compiler */
//#include <io430.h>
#endif

#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "isr_compat.h"
#include "leds.h"
#include "clock.h"
#include "timer.h"
#include "button.h"
#include "uart.h"
#include "adc10.h"
#include "spi.h"
#include "cc2500.h"
#include "flash.h"
#include "watchdog.h"

#include "pt.h"

#include "../../radio/src/main.c"

#include <stdio.h>
#define DBG_PRINTF printf


/* 100 Hz timer A */
#define TIMER_PERIOD_MS 10


#define NODE_ID_LOCATION INFOD_START

#define NODE_ID_UNDEFINED 0x00
/* 10 seconds to reply to an id request */
#define ID_INPUT_TIMEOUT_SECONDS 10
/* the same in timer ticks */
#define ID_INPUT_TIMEOUT_TICKS (ID_INPUT_TIMEOUT_SECONDS*1000/TIMER_PERIOD_MS)
static unsigned char node_id;

#define NUM_TIMERS 5
static uint16_t timer[NUM_TIMERS];
#define TIMER_LED_RED_ON timer[0]
#define TIMER_LED_GREEN_ON timer[1]
#define TIMER_ANTIBOUNCING timer[2]
#define TIMER_TEMP_PRINT timer[3]
#define TIMER_ID_INPUT timer[4]

static void printhex(char *buffer, unsigned int len)
{
    unsigned int i;
    for(i = 0; i < len; i++)
    {
        printf("%02X ", buffer[i]);
    }
}

static void prompt_node_id()
{
    printf("A node requested an id. You have %d seconds to enter an 8-bit ID.\r\n", ID_INPUT_TIMEOUT_SECONDS);
}

/* returns 1 if the id was expected and set, 0 otherwise */
static void set_node_id(unsigned char id)
{
    TIMER_ID_INPUT = UINT_MAX;
    if(flash_write_byte((unsigned char *) NODE_ID_LOCATION, id) != 0)
    {
        flash_erase_segment((unsigned int *) NODE_ID_LOCATION);
        flash_write_byte((unsigned char *) NODE_ID_LOCATION, id);
    }
    node_id = id;
    printf("this node id is now 0x%02X\r\n", id);
}

/* Protothread contexts */

#define NUM_PT 6
static struct pt pt[NUM_PT];


/*
 * Timer
 */

void timer_tick_cb() {
    int i;
    for(i = 0; i < NUM_TIMERS; i++)
    {
        if(timer[i] != UINT_MAX) {
            timer[i]++;
        }
    }
}

int timer_reached(uint16_t timer, uint16_t count) {
    return (timer >= count);
}


/*
 * LEDs
 */

static int led_green_duration;
static int led_green_flag;

/* asynchronous */
static void led_green_blink(int duration)
{
    led_green_duration = duration;
    led_green_flag = 1;
}

static PT_THREAD(thread_led_green(struct pt *pt))
{
    PT_BEGIN(pt);

    while(1)
    {
        PT_WAIT_UNTIL(pt, led_green_flag);
        led_green_on();
        TIMER_LED_GREEN_ON = 0;
        PT_WAIT_UNTIL(pt, timer_reached(TIMER_LED_GREEN_ON,
          led_green_duration));
        led_green_off();
        led_green_flag = 0;
    }

    PT_END(pt);
}

static PT_THREAD(thread_led_red(struct pt *pt))
{
    PT_BEGIN(pt);

    while(1)
    {
        led_red_switch();
        TIMER_LED_RED_ON = 0;
        PT_WAIT_UNTIL(pt, timer_reached(TIMER_LED_RED_ON, 100));
    }

    PT_END(pt);
}





/*
 * UART
 */

static int uart_flag;
static uint8_t uart_data;

int uart_cb(uint8_t data)
{
    uart_flag = 1;
    uart_data = data;
    return 0;
}

/* to be called from within a protothread */
static void print_temperature()
{
    int temperature = adc10_sample_temp();
    printf("temperature: %d, hex: ", temperature);
    printhex((char *) &temperature, 2);
    putchar('\r');
    putchar('\n');
}

static PT_THREAD(thread_uart(struct pt *pt))
{
    PT_BEGIN(pt);

    while(1)
    {
        PT_WAIT_UNTIL(pt, uart_flag);

        led_green_blink(10); /* 10 timer ticks = 100 ms */

        /* does the local node expects an id
         * or do we have to broadcast it? */
	

	printf("uart_data: %d \r\n", uart_data);

	set_node_id(uart_data);

        uart_flag = 0;
    }

    PT_END(pt);
}

/*
 * Button
 */

#define ANTIBOUNCING_DURATION 10 /* 10 timer counts = 100 ms */
static int antibouncing_flag;
static int button_pressed_flag;

void button_pressed_cb()
{
    if(antibouncing_flag == 0)
    {
        button_pressed_flag = 1;
        antibouncing_flag = 1;
        TIMER_ANTIBOUNCING = 0;
        led_green_blink(200); /* 200 timer ticks = 2 seconds */
    }
}

static PT_THREAD(thread_button(struct pt *pt))
{
    PT_BEGIN(pt);

    while(1)
    {
        PT_WAIT_UNTIL(pt, button_pressed_flag == 1);

        TIMER_ID_INPUT = 0;

        /* ask locally for a node id and broadcast an id request */
        prompt_node_id();

        button_pressed_flag = 0;
    }


    PT_END(pt);
}

static PT_THREAD(thread_antibouncing(struct pt *pt))
{
    PT_BEGIN(pt);

    while(1)
    {
        PT_WAIT_UNTIL(pt, antibouncing_flag
          && timer_reached(TIMER_ANTIBOUNCING, ANTIBOUNCING_DURATION));
        antibouncing_flag = 0;
    }

    PT_END(pt);
}

static PT_THREAD(thread_periodic_print(struct pt *pt))
{
    PT_BEGIN(pt);

    while(1)
    {
        TIMER_TEMP_PRINT = 0;
        PT_WAIT_UNTIL(pt, node_id != NODE_ID_UNDEFINED && timer_reached( TIMER_TEMP_PRINT, 1000));
        print_temperature();
    }

    PT_END(pt);
}

/*
 * Radio
 */
static PT_THREAD(thread_tx(struct pt *pt))
{
    PT_BEGIN(pt);

    while(1)
    {
        TIMER_TEMP_PRINT = 0;
        PT_WAIT_UNTIL(pt, node_id != NODE_ID_UNDEFINED && timer_reached( TIMER_TEMP_PRINT, 1000));
        tx();
    }

    PT_END(pt);
}

static PT_THREAD(thread_rx(struct pt *pt))
{
    PT_BEGIN(pt);

    while(1)
    {
        TIMER_TEMP_PRINT = 0;
        PT_WAIT_UNTIL(pt, node_id != NODE_ID_UNDEFINED && timer_reached( TIMER_TEMP_PRINT, 1000));
        rx();
    }

    PT_END(pt);
}


/*
 * main
 */

int main(void)
{
    watchdog_stop();

    TIMER_ID_INPUT = UINT_MAX;
    node_id = NODE_ID_UNDEFINED;

    /* protothreads init */
    int i;
    for(i = 0; i < NUM_PT; i++)
    {
        PT_INIT(&pt[i]);
    }

    /* clock init */
    set_mcu_speed_dco_mclk_16MHz_smclk_8MHz();

    /* LEDs init */
    leds_init();
    led_red_on();
    led_green_flag = 0;

    /* timer init */
    timerA_init();
    timerA_register_cb(&timer_tick_cb);
    timerA_start_milliseconds(TIMER_PERIOD_MS);

    /* button init */
    button_init();
    button_register_cb(button_pressed_cb);
    antibouncing_flag = 0;
    button_pressed_flag = 0;

    /* UART init (serial link) */
    uart_init(UART_9600_SMCLK_8MHZ);
    uart_register_cb(uart_cb);
    uart_flag = 0;
    uart_data = 0;

    /* ADC10 init (temperature) */
    adc10_start();


    /* retrieve node id from flash */
    node_id = *((char *) NODE_ID_LOCATION);
    printf("node id retrieved from flash: %d\r\n", node_id);

    button_enable_interrupt();
    __enable_interrupt();

    /* simple cycle scheduling */
    while(1) {
        thread_led_red(&pt[0]);
        thread_led_green(&pt[1]);
        //thread_uart(&pt[2]);
        //thread_antibouncing(&pt[3]);
        //thread_periodic_print(&pt[4]);
        //thread_button(&pt[5]);
	thread_rx(&pt[2]);
    }
}
