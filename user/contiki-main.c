//C标准头文件
#include <stdint.h>
#include <stdio.h>
//STM32F103相关头文件
#include "stm32f10x.h"
#include "bsp_led.h"
#include "debug-uart.h"
//Contiki相关头文件
#include <clock.h>
#include <sys/process.h>
#include <sys/autostart.h>
#include <etimer.h>

unsigned int idle_count = 0;

//声明了一个进程控制块和一个进程执行体函数process_thread_blink_process
//进程结构体控制块中的函数指针指向声明的函数
PROCESS(blink_process, "Blink");
//将进程加入到进程自启动队列autostart_process[]
AUTOSTART_PROCESSES(&blink_process);
//定义刚才声明的函数process_thread_blink_process
PROCESS_THREAD(blink_process, ev, data)
{
    PROCESS_BEGIN();
    while (1)
    {
        static struct etimer et;
        //开启etimer
        etimer_set(&et, CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        //打开LED
        LED3(ON);
        //printf("LEDON\r\n");
        etimer_set(&et, CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        //关闭LED
        LED3(OFF);
        //printf("LEDOFF\r\n");
    }
    PROCESS_END();
}

int main()
{
    USART_Config();
    LED_GPIO_Config();
    printf("setting the usart successfully!!\r\n");
    //设置SysTick每1/CLOCK_SECOND秒产生一次中断
    clock_init();
    /*进程初始化时，让lastevent=PROCESS_EVENT_MAX，即新产生的事件从0x8b开始，函数process_alloc_event用于分配一个新的事件*/
    process_init();
    process_start(&etimer_process, NULL);
    /*调用process_start()函数挨个启动autostart_processes数组中的进程
  进程少的时候，也可以单独调用process_start，和注释掉的部分等价。*/
    autostart_start(autostart_processes);
    //process_start(&blink_process,NULL);
    while (1)
    {
        do
        {
        } while (process_run() > 0);
        idle_count++;
        /* Idle! */
        /* Stop processor clock */
        /* asm("wfi"::); */
    }
    return 0;
}
