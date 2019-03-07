//C��׼ͷ�ļ�
#include <stdint.h>
#include <stdio.h>
//STM32F103���ͷ�ļ�
#include "stm32f10x.h"
#include "bsp_led.h"
#include "debug-uart.h"
//Contiki���ͷ�ļ�
#include <clock.h>
#include <sys/process.h>
#include <sys/autostart.h>
#include <etimer.h>

unsigned int idle_count = 0;

//������һ�����̿��ƿ��һ������ִ���庯��process_thread_blink_process
//���̽ṹ����ƿ��еĺ���ָ��ָ�������ĺ���
PROCESS(blink_process, "Blink");
//�����̼��뵽��������������autostart_process[]
AUTOSTART_PROCESSES(&blink_process);
//����ղ������ĺ���process_thread_blink_process
PROCESS_THREAD(blink_process, ev, data)
{
    PROCESS_BEGIN();
    while (1)
    {
        static struct etimer et;
        //����etimer
        etimer_set(&et, CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        //��LED
        LED3(ON);
        //printf("LEDON\r\n");
        etimer_set(&et, CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        //�ر�LED
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
    //����SysTickÿ1/CLOCK_SECOND�����һ���ж�
    clock_init();
    /*���̳�ʼ��ʱ����lastevent=PROCESS_EVENT_MAX�����²������¼���0x8b��ʼ������process_alloc_event���ڷ���һ���µ��¼�*/
    process_init();
    process_start(&etimer_process, NULL);
    /*����process_start()������������autostart_processes�����еĽ���
  �����ٵ�ʱ��Ҳ���Ե�������process_start����ע�͵��Ĳ��ֵȼۡ�*/
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
