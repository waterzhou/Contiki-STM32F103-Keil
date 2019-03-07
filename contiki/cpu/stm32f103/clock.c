#include "stm32f10x.h"
#include "stm32f10x_it.h"
//#include "debug-uart.h"
#include <sys/clock.h>
#include <sys/cc.h>
#include <sys/etimer.h>

static volatile clock_time_t current_clock = 0;
static volatile unsigned long current_seconds = 0;
static unsigned int second_countdown = CLOCK_SECOND;

void SysTick_Handler(void)
{
    current_clock++;
    //如果timelist中有etimer到期了( 写成current_clock >= etimer_next_expiration_time()更容易理解 )
    if (etimer_pending() && etimer_next_expiration_time() <= current_clock)
    {
        //把etimer_processes的needpoll和poll_requested设置为1
        etimer_request_poll();
        //printf("%d,%d\r\n",clock_time(),etimer_next_expiration_time     ());
    }
    /*每1/CLOCK_SECOND秒进一次中断，即每1/CLOCK_SECOND秒second_countdown自减一次，因此
  每1秒current_seconds加一次，即current_seconds是秒表*/
    if (--second_countdown == 0)
    {
        current_seconds++;
        second_countdown = CLOCK_SECOND;
    }
}

void clock_init()
{
    //设置每1/CLOCK_SECOND秒产生一次中断
    if (SysTick_Config(SystemCoreClock / CLOCK_SECOND))
    {
        while (1)
            ;
    }
}

clock_time_t clock_time(void)
{
    return current_clock;
}

#if 0
/* The inner loop takes 4 cycles. The outer 5+SPIN_COUNT*4. */

#define SPIN_TIME 2 /* us */
#define SPIN_COUNT (((MCK * SPIN_TIME / 1000000) - 5) / 4)

#ifndef __MAKING_DEPS__

void
clock_delay(unsigned int t)
{
#ifdef __THUMBEL__ 
  asm volatile("1: mov r1,%2\n2:\tsub r1,#1\n\tbne 2b\n\tsub %0,#1\n\tbne 1b\n":"=l"(t):"0"(t),"l"(SPIN_COUNT));
#else
#error Must be compiled in thumb mode
#endif
}
#endif
#endif /* __MAKING_DEPS__ */

unsigned long 
clock_seconds(void)
{
    return current_seconds;
}
