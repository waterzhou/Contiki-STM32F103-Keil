本代码已经将contiki成功移植到stm32f103ze上
基于Keil 5，将本代码下载到本地，然后用keil 5打开
直接下载到STM32上便可直接运行。
代码的功能：使LED闪烁。
移植过程：
首先参考野火单片机那本《零死角玩转STM32F103》用固件库点亮LED
灯那一节，新建一个能让LED灯闪烁的工程。
然后再将contiki部分源码文件添加进来，
至于添加了哪些，自己参考问件夹contiki。
然后修改clock.c中的clock_init()函数和SysTick_Handler（）函数。
把stm32f10x_it.c中的SysTick_Handler()函数删掉。
2.在contiki-conf.h文件中添加如下代码来使能进程自动启动：
#define AUTOSTART_ENABLE 1
这样才能使用autostart_processes这个来作为参数。
至此移植完成。