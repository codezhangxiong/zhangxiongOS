#include "bootpack.h"
#define PIT_CTRL	0x0043
#define PIT_CNT0	0x0040

#define TIMER_FLAGS_ALLOC	1
#define TIMER_FLAGS_USING	2

struct TIMERCTL timerctl;

void init_pit(void)
{
	int i;
	io_out8(PIT_CTRL, 0x34);
	io_out8(PIT_CNT0, 0x9c);
	io_out8(PIT_CNT0, 0x2e);
	timerctl.count = 0;
	timerctl.next = 0xffffffff;
	timerctl.using = 0;
	for (i = 0; i < MAX_TIMER; i++) {
		timerctl.timers0[i].flags = 0;
	}
	return;
}

struct TIMER* timer_alloc(void)
{
	int i;
	for (i = 0; i < MAX_TIMER; i++)
	{
		if (timerctl.timers0[i].flags == 0) {
			timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
			return &timerctl.timers0[i];
		}
	}
	return 0;
}

void timer_free(struct TIMER* timer)
{
	timer->flags = 0;
	return;
}

void timer_init(struct TIMER* timer, struct FIFO32* fifo, int data)
{
	timer->fifo = fifo;
	timer->data = data;
	return;
}

void timer_settime(struct TIMER* timer, unsigned int timeout)
{
	int e;
	struct TIMER* t, * s;
	timer->timeout = timeout + timerctl.count;
	timer->flags = TIMER_FLAGS_USING;
	e = io_load_eflags();
	io_cli();
	timerctl.using++;
	if (timerctl.using == 1)
	// 处于运行状态的定时器只有这一个时
	{
		timerctl.t0 = timer;
		timer->next = 0;
		timerctl.next = timer->timeout;
		io_store_eflags(e);
		return;
	}
	t = timerctl.t0;
	if (timer->timeout <= t->timeout)
	// 插入最前面的情况下
	{
		timerctl.t0 = timer;
		timer->next = t;
		timerctl.next = timer->timeout;
		io_store_eflags(e);
		return;
	}
	for (;;)
	{
		s = t;
		t = t->next;
		if (t == 0) { break; }
		if (timer->timeout <= t->timeout)
		// 插入到s和t之间
		{
			s->next = timer; // s的下一个是timer
			timer->next = t; // timer的下一个是t
			io_store_eflags(e);
			return;
		}
	}
	// 插入最后面的情况下
	s->next = timer;
	timer->next = 0;
	io_store_eflags(e);
	return;
}

void inthandler20(int* esp)
{
	int i;
	struct TIMER* timer;
	io_out8(PIC0_OCW2, 0x60); // 把IRQ-00接收信号结束的信息通知给PIC
	timerctl.count++;
	if (timerctl.next > timerctl.count) { return; }
	timer = timerctl.t0; // 首先把最前面的地址赋给timer
	for (i = 0; i < timerctl.using; i++)
	{
		if (timer->timeout > timerctl.count) { break; }
		timer->flags = TIMER_FLAGS_ALLOC;
		fifo32_put(timer->fifo, timer->data);
		timer = timer->next;
	}
	timerctl.using -= i;
	// 新位移
	timerctl.t0 = timer;
	//timerctl.next = timerctl.using > 0 ? timerctl.timers[0]->timeout : 0xffffffff;
	if (timerctl.using > 0) {
		timerctl.next = timerctl.t0->timeout;
	}
	else {
		timerctl.next = 0xffffffff;
	}
	return;
}
//void inthandler20(int* esp)
//{
//	int i;
//	char ts = 0;
//	struct TIMER* timer;
//	io_out8(PIC0_OCW2, 0x60); // 把IRQ-00接收信号结束的信息通知给PIC
//	timerctl.count++;
//	if (timerctl.next > timerctl.count) { return; }
//	timer = timerctl.t0; // 首先把最前面的地址赋给timer
//	for (i = 0; i < timerctl.using; i++)
//	{
//		if (timer->timeout > timerctl.count) { break; }
//		timer->flags = TIMER_FLAGS_ALLOC;
//		if(timer!=mt_timer){ fifo32_put(timer->fifo, timer->data); }
//		else { ts = 1; } // 将下一个计时器的地址赋给timer
//		timer = timer->next;
//	}
//	timerctl.using -= i;
//	// 新位移
//	timerctl.t0 = timer;
//	timerctl.next = timerctl.using > 0 ? timerctl.timers[0]->timeout : 0xffffffff;
//
//	return;
//}
