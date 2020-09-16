#include <stdio.h>
#include "bootpack.h"

struct TSS32 {
	int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
	int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
	int es, cs, ss, ds, fs, gs;
	int ldtr, iomap;
};
// 多任务 - b任务
void task_b_main(struct SHEET* sht_back);

void make_window8(unsigned char* buf, int xsize, int ysize, char* title);

void make_textbox8(struct SHEET* sht, int x0, int y0, int sx, int sy, int c);
// 参数说明：图层sht,坐标(x,y),字体颜色c,背景颜色b,字符串s,字符个数l
void putfont8_asc_sht(struct SHEET* sht, int x, int y, int c, int b, char* s, int l);
// 键盘表
static char keytable[0x54] = {
	0,0,'1','2','3','4','5','6','7','8','9','0','-','^',0,0,
	'Q','W','E','R','T','Y','U','I','O','P','@','[',0,0,'A','S',
	'D','F','G','H','J','K','L',';',':',0,0,']','Z','X','C','V',
	'B','N','M',',','.','/','0','*',0,' ',0,0,0,0,0,0,
	0,0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1',
	'2','3','0','.'
};

void HariMain(void)
{
	// 初始化
	init_gdtidt();
	init_pic();
	io_sti();
	init_pit();
	// 内存管理初始化
	struct MEMMAN* memman = (struct MEMMAN*)MEMMAN_ADDR;
	unsigned int memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000);
	memman_free(memman, 0x00400000, memtotal - 0x00400000);
	// 缓冲区初始化
	struct FIFO32 fifo;
	int fifobuf[128];
	fifo32_init(&fifo, 128, fifobuf);
	init_keyboard(&fifo, 256);

	struct MOUSE_DEC mdec;
	enable_mouse(&fifo, 512, &mdec);
	io_out8(PIC0_IMR, 0xf8); // pit、pic1和键盘设置许可(11111000)
	io_out8(PIC1_IMR, 0xef); // 鼠标设置许可(11101111)
	// 定时器
	struct TIMER* timer = timer_alloc();
	timer_init(timer, &fifo, 10);
	timer_settime(timer, 1000);

	struct TIMER* timer2 = timer_alloc();
	timer_init(timer2, &fifo, 3);
	timer_settime(timer2, 300);

	struct TIMER* timer3 = timer_alloc();
	timer_init(timer3, &fifo, 1);
	timer_settime(timer3, 50);
	/* --------显示-------- */
	init_palette();
	struct BOOTINFO* binfo = (struct BOOTINFO*)ADR_BOOTINFO;
	struct SHTCTL* shtctl;
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	// 背景显示
	struct SHEET* sht_back = sheet_alloc(shtctl);
	unsigned char* buf_back = (unsigned char*)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);
	init_screen(buf_back, binfo->scrnx, binfo->scrny);
	sheet_slide(sht_back, 0, 0);
	sheet_updown(sht_back, 0);
	// 文字显示
	char s[40];
	sprintf(s, "memory %dMB    free:%dKB", memtotal / (1024 * 1024), memman_total(memman) / 1024);
	putfont8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFF00, s);
	// 窗口
	struct SHEET* sht_win = sheet_alloc(shtctl);
	unsigned char* buf_win = (unsigned char*)memman_alloc_4k(memman, 160 * 52);
	sheet_setbuf(sht_win, buf_win, 160, 52, -1);
	make_window8(buf_win, 160, 52, "window");
	make_textbox8(sht_win, 8, 28, 144, 16, COL8_FFFFFF);
	sheet_slide(sht_win, 80, 72);
	sheet_updown(sht_win, 1);
	// 鼠标显示
	struct SHEET* sht_mouse = sheet_alloc(shtctl);
	unsigned char buf_mouse[256];
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);
	init_mouse_cursor8(buf_mouse, 99);
	int mx = (binfo->scrnx - 16) / 2;
	int my = (binfo->scrny - 28 - 16) / 2;
	sheet_slide(sht_mouse, mx, my);
	sheet_updown(sht_mouse, 2);
	sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48);

	// 多任务
	struct TSS32 tss_a, tss_b;
	struct SEGMENT_DESCRIPTOR* gdt = (struct SEGMENT_DESCRIPTOR*)ADR_GDT;
	int task_b_esp;
	struct TIMER* timer_ts = timer_alloc();
	timer_init(timer_ts, &fifo, 2);
	timer_settime(timer_ts, 2);

	tss_a.ldtr = 0;
	tss_a.iomap = 0x40000000;
	tss_b.ldtr = 0;
	tss_b.iomap = 0x40000000;
	set_segmdesc(gdt + 3, 103, (int)&tss_a, AR_TSS32);
	set_segmdesc(gdt + 4, 103, (int)&tss_b, AR_TSS32);
	load_tr(3 * 8);
	task_b_esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 8;
	tss_b.eip = (int)&task_b_main;
	tss_b.eflags = 0x00000202;
	tss_b.eax = 0;
	tss_b.ecx = 0;
	tss_b.edx = 0;
	tss_b.ebx = 0;
	tss_b.esp = task_b_esp;
	tss_b.ebp = 0;
	tss_b.esi = 0;
	tss_b.edi = 0;
	tss_b.es = 1 * 8;
	tss_b.cs = 2 * 8;
	tss_b.ss = 1 * 8;
	tss_b.ds = 1 * 8;
	tss_b.fs = 1 * 8;
	tss_b.gs = 1 * 8;
	*((int*)(task_b_esp + 4)) = (int)sht_back;

	int cursor_x = 8;
	int cursor_c = COL8_FFFFFF;
	for (;;)
	{
		io_cli();

		if (fifo32_status(&fifo) == 0) { io_sti(); }
		else
		{
			int i = fifo32_get(&fifo);
			io_sti();
			if (i == 2)
			{
				farjmp(0, 4 * 8);
				timer_settime(timer_ts, 2);
			}
			// 键盘
			if (256 <= i && i < 511)
			{
				sprintf(s, "%02X", i - 256);
				putfont8_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);
				if (i <256+0x54)
				{
					if (keytable[i - 256] != 0)
					{
						s[0] = keytable[i - 256];
						s[1] = 0;
						putfont8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
						cursor_x += 8;
					}
				}
				if (i == 256 + 0x0e && cursor_x > 8)
				{
					putfont8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
					cursor_x -= 8;
				}
			}
			// 鼠标
			else if (512 <= i && i <= 767)
			{
				if (mouse_decode(&mdec, i - 512) != 0)
				{
					// 鼠标按键
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if ((mdec.btn & 0x01) != 0) { s[1] = 'L'; }
					if ((mdec.btn & 0x02) != 0) { s[3] = 'R'; }
					if ((mdec.btn & 0x04) != 0) { s[2] = 'C'; }
					putfont8_asc_sht(sht_back, 32, 16, COL8_FFFFFF, COL8_FF00FF, s, 15);
					// 鼠标移动
					mx += mdec.x; my += mdec.y;
					if (mx < 0) { mx = 0; }
					if (my < 0) { my = 0; }
					if (mx > binfo->scrnx - 1) { mx = binfo->scrnx - 1; }
					if (my > binfo->scrny - 1) { my = binfo->scrny - 1; }
					sprintf(s, "(%3d,%3d)", mx, my);
					putfont8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_FF00FF, s, 10);
					sheet_slide(sht_mouse, mx, my);
					if ((mdec.btn & 0x01) != 0)
					// 按下左键，移动sht_win
					{
						sheet_slide(sht_win, mx - 80, my - 8);
					}
				}
			}
			else if (i == 10)
			{
				putfont8_asc_sht(sht_back, 0, 64, COL8_FFFFFF, COL8_008484, "10[sec]", 7);
				taskswitch4();
			}
			else if (i == 3)
			{
				putfont8_asc_sht(sht_back, 0, 80, COL8_FFFFFF, COL8_008484, "3[sec]", 6);
			}
			else if (i == 1 || i == 0)
			{
				if (i == 1) {
					timer_init(timer3, &fifo, 0);
					cursor_c = COL8_000000;
				}
				else {
					timer_init(timer3, &fifo, 1);
					cursor_c = COL8_FFFFFF;
				}
				timer_settime(timer3, 50);
				boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			}
		}
	}
}

void task_b_main(struct SHEET* sht_back)
{
	struct FIFO32 fifo;
	struct TIMER* timer_ts, * timer_put, * timer_ls;
	int i, fifobuf[128], count = 0, count0 = 0;
	char s[12];

	fifo32_init(&fifo, 128, fifobuf);
	timer_ts = timer_alloc();
	timer_init(timer_ts, &fifo, 2);
	timer_settime(timer_ts, 2);
	timer_put = timer_alloc();
	timer_init(timer_put, &fifo, 1);
	timer_settime(timer_put, 1);
	timer_ls = timer_alloc();
	timer_init(timer_ls, &fifo, 100);
	timer_settime(timer_ls, 100);

	for (;;)
	{
		count++;
		io_cli();
		if (fifo32_status(&fifo) == 0)
		{
			io_stihlt();
		}
		else
		{
			i = fifo32_get(&fifo);
			io_sti();
			if (i == 1)
			{
				sprintf(s, "%11d", count);
				putfont8_asc_sht(sht_back, 0, 144, COL8_FFFFFF, COL8_008484, s, 11);
				timer_settime(timer_put, 1);
			}
			else if (i == 2)
			{
				farjmp(0, 3 * 8);
				timer_settime(timer_ts, 2);
			}
			else if (i == 100)
			{
				sprintf(s, "%11d", count - count0);
				putfont8_asc_sht(sht_back, 0, 128, COL8_FFFFFF, COL8_008484, s, 11);
				count0 = count;
				timer_settime(timer_ls, 100);
			}
		}
	}
}

void make_window8(unsigned char* buf, int xsize, int ysize, char* title)
{
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, xsize - 1, 0);
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, xsize - 2, 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, 0, ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, 1, ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0, xsize - 1, ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2, 2, xsize - 3, ysize - 3);
	boxfill8(buf, xsize, COL8_000084, 3, 3, xsize - 4, 20);
	boxfill8(buf, xsize, COL8_848484, 1, ysize - 2, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0, ysize - 1, xsize - 1, ysize - 1);

	static char closebtn[14][16] = {
	"ooooooooooooooo@",
	"oQQQQQQQQQQQQQ$@",
	"oQQQQQQQQQQQQQ$@",
	"oQQQ@@QQQQ@@QQ$@",
	"oQQQQ@@QQ@@QQQ$@",
	"oQQQQQ@@@@QQQQ$@",
	"oQQQQQQ@@QQQQQ$@",
	"oQQQQQ@@@@QQQQ$@",
	"oQQQQ@@QQ@@QQQ$@",
	"oQQQ@@QQQQ@@QQ$@",
	"oQQQQQQQQQQQQQ$@",
	"oQQQQQQQQQQQQQ$@",
	"o$$$$$$$$$$$$$$@",
	"@@@@@@@@@@@@@@@@"
	};
	char c;
	int x, y;
	for (y = 0; y < 14; y++)
	{
		for (x = 0; x < 16; x++)
		{
			c = closebtn[y][x];
			if (c == '@') { c = COL8_000000; }
			else if (c == '$') { c = COL8_848484; }
			else if (c == 'Q') { c = COL8_C6C6C6; }
			else { c = COL8_FFFFFF; }
			buf[(5 + y) * xsize + (xsize - 21 + x)] = c;
		}
	}

	putfont8_asc(buf, xsize, 24, 4, COL8_FFFFFF, title);

	return;
}

void make_textbox8(struct SHEET* sht, int x0, int y0, int sx, int sy, int c)
{
	int x1 = x0 + sx, y1 = y0 + sy;
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 2, y0 - 3, x1 + 1, y0 - 3);
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 3, y0 - 3, x0 - 3, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x0 - 3, y1 + 2, x1 + 1, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x1 + 2, y0 - 3, x1 + 2, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 1, y0 - 2, x1 + 0, y0 - 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 2, y0 - 2, x0 - 2, y1 + 0);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x0 - 2, y1 + 1, x1 + 0, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x1 + 1, y0 - 2, x1 + 1, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, c, x0 - 1, y0 - 1, x1 + 0, y1 + 0);
}

void putfont8_asc_sht(struct SHEET* sht, int x, int y, int c, int b, char* s, int l)
{
	boxfill8(sht->buf, sht->bxsize, b, x, y, x + l * 8 - 1, y + 15);
	putfont8_asc(sht->buf, sht->bxsize, x, y, c, s);
	sheet_refresh(sht, x, y, x + l * 8, y + 16);
	return;
}
