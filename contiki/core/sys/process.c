/*
 * Copyright (c) 2005, Swedish Institute of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \addtogroup process
 * @{
 */

/**
 * \file
 *         Implementation of the Contiki process kernel.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */

#include <stdio.h>

#include "sys/arg.h"
#include "sys/process.h"

/*
 * Pointer to the currently running process structure.
 */
struct process *process_list = NULL;
struct process *process_current = NULL;

static process_event_t lastevent;

/*
 * Structure used for keeping the queue of active events.
 */
struct event_data
{
    process_event_t ev;
    process_data_t data;
    struct process *p;
};

static process_num_events_t nevents, fevent;
static struct event_data events[PROCESS_CONF_NUMEVENTS];

#if PROCESS_CONF_STATS
process_num_events_t process_maxevents;
#endif

static volatile unsigned char poll_requested;

#define PROCESS_STATE_NONE 0
#define PROCESS_STATE_RUNNING 1
#define PROCESS_STATE_CALLED 2

static void
call_process(struct process *p, process_event_t ev, process_data_t data);

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
process_event_t
process_alloc_event(void)
{
    return lastevent++;
}
/*---------------------------------------------------------------------------*/
/*判断进程是否在进程链表中，如果不在，那么将进程加入到进程链表头部
设置进程的状态为RUNNING状态（将进程投入到运行队列中）,将进程保存断点的变量设置为0

*/
void process_start(struct process *p, const char *arg)
{
    struct process *q;

    /* First make sure that we don't try to start a process that is
   already running. */
    for (q = process_list; q != p && q != NULL; q = q->next)
        ;
    /* If we found the process on the process list, we bail out. */
    if (q == p)
    {
        return;
    }
    /* Put on the procs list.*/
    //总是把新进程添加到进程链表的头部
    p->next = process_list;
    process_list = p;
    p->state = PROCESS_STATE_RUNNING;
    //让记录进程执行的行号初始值为0,即lc = 0
    PT_INIT(&p->pt);

    PRINTF("process: starting '%s'\n", PROCESS_NAME_STRING(p));

    /* Post a synchronous initialization event to the process. */
    //执行当前进程P
    process_post_synch(p, PROCESS_EVENT_INIT, (process_data_t)arg);
}
/*---------------------------------------------------------------------------*/
static void exit_process(struct process *p, struct process *fromprocess)
{
    register struct process *q;
    struct process *old_current = process_current;

    PRINTF("process: exit_process '%s'\n", PROCESS_NAME_STRING(p));

    /* Make sure the process is in the process list before we try to
 exit it. */
    for (q = process_list; q != p && q != NULL; q = q->next)
        ;
    if (q == NULL)
    {
        return;
    }

    if (process_is_running(p))
    {
        /* Process was running */
        p->state = PROCESS_STATE_NONE;

        /*
     * Post a synchronous event to all processes to inform them that
     * this process is about to exit. This will allow services to
     * deallocate state associated with this process.
     */
        for (q = process_list; q != NULL; q = q->next)
        {
            if (p != q)
            {
                call_process(q, PROCESS_EVENT_EXITED, (process_data_t)p);
            }
        }

        if (p->thread != NULL && p != fromprocess)
        {
            /* Post the exit event to the process that is about to exit. */
            process_current = p;
            p->thread(&p->pt, PROCESS_EVENT_EXIT, NULL);
        }
    }

    if (p == process_list)
    {
        process_list = process_list->next;
    }
    else
    {
        for (q = process_list; q != NULL; q = q->next)
        {
            if (q->next == p)
            {
                q->next = p->next;
                break;
            }
        }
    }

    process_current = old_current;
}
/*---------------------------------------------------------------------------*/
static void call_process(struct process *p, process_event_t ev,
                         process_data_t data)
{
    int ret;

#if DEBUG
    if (p->state == PROCESS_STATE_CALLED)
    {
        printf("process: process '%s' called again with event %d\n",
               PROCESS_NAME_STRING(p), ev);
    }
#endif /* DEBUG */

    if ((p->state & PROCESS_STATE_RUNNING) && p->thread != NULL)
    {
        PRINTF("process: calling process '%s' with event %d\n",
               PROCESS_NAME_STRING(p), ev);
        process_current = p;
        p->state = PROCESS_STATE_CALLED;
        //执行进程的主体
        ret = p->thread(&p->pt, ev, data);
        if (ret == PT_EXITED || ret == PT_ENDED || ev == PROCESS_EVENT_EXIT)
        {
            exit_process(p, p);
        }
        else
        {
            p->state = PROCESS_STATE_RUNNING;
        }
    }
}
/*---------------------------------------------------------------------------*/
void process_exit(struct process *p)
{
    exit_process(p, PROCESS_CURRENT());
}
/*---------------------------------------------------------------------------*/
void process_init(void)
{
    /*初始化事件队列
系统自身定义了10个事件（编号从0x80到0x8a），进程初始化时，
让lastevent=PROCESS_EVENT_MAX，即新产生的事件从0x8b开始，
函数process_alloc_event用于分配一个新的事件
*/
    lastevent = PROCESS_EVENT_MAX;
    nevents = fevent = 0; //让未处理事件和将要处理事件的下标为0
#if PROCESS_CONF_STATS
    process_maxevents = 0;
#endif /* PROCESS_CONF_STATS */
    //初始化进程链表，即将进程链表头指向为空，当前进程也设为空
    process_current = process_list = NULL;
}
/*---------------------------------------------------------------------------*/
/*
 * Call each process' poll handler.
 */
/*---------------------------------------------------------------------------*/
static void
do_poll(void)
{
    struct process *p;

    poll_requested = 0;
    /* Call the processes that needs to be polled. */
    for (p = process_list; p != NULL; p = p->next)
    {
        if (p->needspoll)
        {
            p->state = PROCESS_STATE_RUNNING;
            p->needspoll = 0;
            call_process(p, PROCESS_EVENT_POLL, NULL);
        }
    }
}
/*---------------------------------------------------------------------------*/
/*
 * Process the next event in the event queue and deliver it to
 * listening processes.
 */
/*---------------------------------------------------------------------------*/
static void
do_event(void)
{
    static process_event_t ev;
    static process_data_t data;
    static struct process *receiver;
    static struct process *p;

    /*
   * If there are any events in the queue, take the first one and walk
   * through the list of processes to see if the event should be
   * delivered to any of them. If so, we call the event handler
   * function for the process. We only process one event at a time and
   * call the poll handlers inbetween.
   */

    if (nevents > 0)
    {
        /* There are events that we should deliver. */
        ev = events[fevent].ev;

        data = events[fevent].data;
        receiver = events[fevent].p;

        /* Since we have seen the new event, we move pointer upwards
   and decrese the number of events. */
        //这里之所以采用加1取余的方式是因为事件队列是循环队列,到了最大值后又从0开始
        fevent = (fevent + 1) % PROCESS_CONF_NUMEVENTS;
        --nevents;

        /* If this is a broadcast event, we deliver it to all events, in
   order of their priority. */
        if (receiver == PROCESS_BROADCAST)
        {
            for (p = process_list; p != NULL; p = p->next)
            {
                /* If we have been requested to poll a process, we do this in
   between processing the broadcast event. */
                if (poll_requested)
                {
                    do_poll();
                }
                call_process(p, ev, data);
            }
        }
        else
        {
            /* This is not a broadcast event, so we deliver it to the
   specified process. */
            /* If the event was an INIT event, we should also update the
   state of the process. */
            if (ev == PROCESS_EVENT_INIT)
            {
                receiver->state = PROCESS_STATE_RUNNING;
            }

            /* Make sure that the process actually is running. */
            call_process(receiver, ev, data);
        }
    }
}
/*---------------------------------------------------------------------------*/
int process_run(void)
{
    /* Process poll events. */
    if (poll_requested)
    {
        do_poll();
    }

    /* Process one event from the queue */
    do_event();

    return nevents + poll_requested;
}
/*---------------------------------------------------------------------------*/
int process_nevents(void)
{
    return nevents + poll_requested;
}
/*---------------------------------------------------------------------------*/
int process_post(struct process *p, process_event_t ev, process_data_t data)
{
    static process_num_events_t snum;

    if (PROCESS_CURRENT() == NULL)
    {
        PRINTF("process_post: NULL process posts event %d to process '%s', nevents "
               "%d\n",
               ev,
               PROCESS_NAME_STRING(p),
               nevents);
    }
    else
    {
        PRINTF("process_post: Process '%s' posts event %d to process '%s', nevents "
               "%d\n",
               PROCESS_NAME_STRING(PROCESS_CURRENT()),
               ev,
               p == PROCESS_BROADCAST ? "<broadcast>" : PROCESS_NAME_STRING(p),
               nevents);
    }

    if (nevents == PROCESS_CONF_NUMEVENTS)
    {
#if DEBUG
        if (p == PROCESS_BROADCAST)
        {
            printf("soft panic: event queue is full when broadcast event %d was "
                   "posted from %s\n",
                   ev,
                   PROCESS_NAME_STRING(process_current));
        }
        else
        {
            printf("soft panic: event queue is full when event %d was posted to %s "
                   "frpm %s\n",
                   ev,
                   PROCESS_NAME_STRING(p),
                   PROCESS_NAME_STRING(process_current));
        }
#endif /* DEBUG */
        return PROCESS_ERR_FULL;
    }

    snum = (process_num_events_t)(fevent + nevents) % PROCESS_CONF_NUMEVENTS;
    events[snum].ev = ev;
    events[snum].data = data;
    events[snum].p = p;
    ++nevents;

#if PROCESS_CONF_STATS
    if (nevents > process_maxevents)
    {
        process_maxevents = nevents;
    }
#endif /* PROCESS_CONF_STATS */

    return PROCESS_ERR_OK;
}
/*---------------------------------------------------------------------------*/
/*
process_post_synch为进程同步函数，内部直接调用call_process函数
将进程投入运行
和process_post_synch对应的为process_post，该函数为进程异步函数，
调用process_post函数处理事件时，事件并未立即处理，而是被加入到事件
等待处理队列nevents里，等待下一次process_run的时候，通过do_event函数来执行。
*/
void process_post_synch(struct process *p, process_event_t ev, process_data_t data)
{
    //考虑到进程运行过程中可能会被中断，因此先保存当前进程指针，执行完再恢复
    struct process *caller = process_current;
    //执行进程p
    call_process(p, ev, data);
    process_current = caller;
}
/*---------------------------------------------------------------------------*/
void process_poll(struct process *p)
{
    if (p != NULL)
    {
        if (p->state == PROCESS_STATE_RUNNING || p->state == PROCESS_STATE_CALLED)
        {
            p->needspoll = 1;
            poll_requested = 1;
        }
    }
}
/*---------------------------------------------------------------------------*/
int process_is_running(struct process *p)
{
    return p->state != PROCESS_STATE_NONE;
}
/*---------------------------------------------------------------------------*/
/** @} */
