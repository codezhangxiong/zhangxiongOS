#include "bootpack.h"

void memman_init(struct MEMMAN* man)
{
	man->frees = 0;
	man->maxfrees = 0;
	man->lostsize = 0;
	man->losts = 0;
	return;
}
// ��������ڴ��С�ĺϼ�
unsigned int memman_total(struct MEMMAN* man)
{
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; i++) {
		t += man->free[i].size;
	}
	return t;
}
// ���䣨4k��
unsigned int memman_alloc_4k(struct MEMMAN* man, unsigned int size)
{
	unsigned int a;
	size = (size + 0x0fff) & 0xfffff000;
	a = memman_alloc(man, size);
	return a;
}
// ����
unsigned int memman_alloc(struct MEMMAN* man, unsigned int size)
{
	unsigned int i, a;
	for (i = 0; i < man->frees; i++)
	{
		if (man->free[i].size >= size)//�ҵ��㹻����ڴ�
		{
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0)//���free[i]Ϊ0��,����һ��������Ϣ
			{
				man->frees--;
				for (; i < man->frees; i++) {
					man->free[i] = man->free[i + 1];
				}
			}
			return a;
		}
	}
	return 0;
}
// �ͷţ�4k��
int memman_free_4k(struct MEMMAN* man,unsigned int addr, unsigned int size)
{
	int i;
	size = (size + 0x0fff) & 0xfffff000;
	i = memman_free(man, addr, size);
	return i;
}
// �ͷ�
int memman_free(struct MEMMAN* man, unsigned int addr, unsigned int size)
{
	int i, j;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].addr > addr) {
			break;
		}
	}
	// free[i-1].addr<addr<free[i].addr
	if (i > 0)
	{
		if (man->free[i - 1].addr + man->free[i - 1].size == addr)
		{
			man->free[i - 1].size += size;
			if (i < man->frees)
			{
				if (addr + size == man->free[i].addr)
				{
					man->free[i - 1].size += man->free[i].size;
					man->frees--;
					for (; i < man->frees; i++) {
						man->free[i] = man->free[i + 1];
					}
				}
			}
			return 0;
		}
	}
	if (i < man->frees) {
		if (addr + size == man->free[i].addr) {
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0;
		}
	}
	if (man->frees < MEMMAN_FREES)
	{
		for (j = man->frees; j < i; j--) {
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees;
		}
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0;
	}
	man->losts++;
	man->lostsize += size;
	return -1;
}
// �ڴ���
unsigned int memtest(unsigned int start, unsigned int end)
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;

	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT;
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	// �����386����ʹ�趨AC=1��AC��ֵ�����Զ��ص�1
	if ((eflg & EFLAGS_AC_BIT) != 0) { flg486 = 1; }
	eflg &= ~EFLAGS_AC_BIT;
	io_store_eflags(eflg);

	if (flg486 != 0)
	{
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; // ��ֹ����
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if (flg486 != 0)
	{
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; // ��������
		store_cr0(cr0);
	}

	return i;
}