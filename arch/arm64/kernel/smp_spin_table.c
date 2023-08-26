// SPDX-License-Identifier: GPL-2.0-only
/*
 * Spin Table SMP initialisation
 *
 * Copyright (C) 2013 ARM Ltd.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/smp.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/t00ls.h>

#include <asm/cacheflush.h>
#include <asm/cpu_ops.h>
#include <asm/cputype.h>
#include <asm/io.h>
#include <asm/smp_plat.h>

extern void secondary_holding_pen(void);
volatile unsigned long __section(".mmuoff.data.read")
secondary_holding_pen_release = INVALID_HWID;

// secondary_startupに遷移した際の書き込みが行われたかどうかのフラグ
// キャッシュを防ぐため？コアごとに用意する
volatile unsigned long __section(".mmuoff.data.read")
first_core_is_released_flag;
volatile unsigned long __section(".mmuoff.data.read")
second_core_is_released_flag;
volatile unsigned long __section(".mmuoff.data.read")
third_core_is_released_flag;

static phys_addr_t cpu_release_addr[NR_CPUS];

/*
 * Write secondary_holding_pen_release in a way that is guaranteed to be
 * visible to all observers, irrespective of whether they're taking part
 * in coherency or not.  This is necessary for the hotplug code to work
 * reliably.
 */
static void write_pen_release(u64 val)
{
	void *start = (void *)&secondary_holding_pen_release;
	unsigned long size = sizeof(secondary_holding_pen_release);

	// ロックしているpen?の番地を確認したい
	printk("write_pen_release: releasing core %llu\n", val);
	// need to %px instead of %p?
 	printk("write_pen_release: the address of secondary_holding_pen_release is %px\n", start);
	printk("write_pen_release: secondary_holding_pen_release is %lu\n", *(unsigned long *)start);

	secondary_holding_pen_release = val;
	dcache_clean_inval_poc((unsigned long)start, (unsigned long)start + size);
}


static int smp_spin_table_cpu_init(unsigned int cpu)
{
	struct device_node *dn;
	int ret;

	dn = of_get_cpu_node(cpu, NULL);
	if (!dn) {
		printk("smp_spin_table_cpu_init: failed to get the device node for cpu %u\n", cpu);
		return -ENODEV;
	}

	/*
	 * Determine the address from which the CPU is polling.
	 */
	ret = of_property_read_u64(dn, "cpu-release-addr",
				   &cpu_release_addr[cpu]);
	if (ret)
		printk("smp_spin_table_cpu_init: missing or invalid cpu-release-addr property for cpu %u\n", cpu);

	of_node_put(dn);

	printk("smp_spin_table_cpu_init: initializing cpu %u\n", cpu);
	printk("smp_spin_table_cpu_init: the cpu's cpu-release-addr is %pa\n", cpu_release_addr[cpu]);

	return ret;
}

static int smp_spin_table_cpu_prepare(unsigned int cpu)
{
	__le64 __iomem *release_addr;
	phys_addr_t pa_holding_pen = __pa_symbol(secondary_holding_pen);

	printk("smp_spin_table_cpu_prepare: secondary_holding_pen addr is %pa", pa_holding_pen);

	if (!cpu_release_addr[cpu]) {
		printk("smp_spin_table_cpu_prepare: cpu_release_addr for cpu %u not set", cpu);
		return -ENODEV;
	}

	/*
	 * The cpu-release-addr may or may not be inside the linear mapping.
	 * As ioremap_cache will either give us a new mapping or reuse the
	 * existing linear mapping, we can use it to cover both cases. In
	 * either case the memory will be MT_NORMAL.
	 */
	release_addr = ioremap_cache(cpu_release_addr[cpu],
				     sizeof(*release_addr));
	if (!release_addr) {
		printk("smp_spin_table_cpu_prepare: failed to io_remap cpu %u's cpu_release_addr", cpu);
		return -ENODEV;
	}

	printk("smp_spin_table_cpu_prepare: io_remapped cpu %u's cpu_release_addr is %llu", cpu, release_addr);

	/*
	 * We write the release address as LE regardless of the native
	 * endianness of the kernel. Therefore, any boot-loaders that
	 * read this address need to convert this address to the
	 * boot-loader's endianness before jumping. This is mandated by
	 * the boot protocol.
	 */
	writeq_relaxed(pa_holding_pen, release_addr);
	dcache_clean_inval_poc((__force unsigned long)release_addr,
			    (__force unsigned long)release_addr +
				    sizeof(*release_addr));

	printk("smp_spin_table_cpu_prepare: io_remapped cpu %u' now has pa_holding_pen of %pa", cpu, *release_addr);

	/*
	 * Send an event to wake up the secondary CPU.
	 */
	sev();

	iounmap(release_addr);

	return 0;
}

static int smp_spin_table_cpu_boot(unsigned int cpu)
{
	void *start = (void *)&secondary_holding_pen_release;
	void *first = (void *)&first_core_is_released_flag;
	void *second = (void *)&second_core_is_released_flag;
	void *third = (void *)&third_core_is_released_flag;

	// どのコアが処理しているのか、たぶんプライマリ
	print_core_id();
	
	// int cpuに紐づくhwidを確かめる
	printk("smp_spin_table_cpu_boot: suppose to release %u", cpu);
	printk("smp_spin_table_cpu_boot: actually releasing %llu", cpu_logical_map(cpu));

	/*
	 * Update the pen release flag.
	 */
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Send an event, causing the secondaries to read pen_release.
	 */
 	printk("smp_spin_table_cpu_boot: signaling that the pen is released\n");
 	sev();

	// 解放されてsecondary_startupに遷移している場合は、secondary_holding_pen_releaseは1になるはず
	udelay(10);
 	printk("smp_spin_table_cpu_boot: signal was sent, the secondary_holding_pen_release is %lu\n", *(unsigned long *)start);

	// キャッシュされるの？
	if (cpu == 1) {
		printk("smp_spin_table_cpu_boot: is the first core released %lu\n", *(unsigned long *)first);
	}
	else if(cpu == 2) {
		printk("smp_spin_table_cpu_boot: is the second core released %lu\n", *(unsigned long *)second);
	}
	else {
		printk("smp_spin_table_cpu_boot: is the third core released %lu\n", *(unsigned long *)third);
	}

	return 0;
}

const struct cpu_operations smp_spin_table_ops = {
	.name		= "spin-table",
	.cpu_init	= smp_spin_table_cpu_init,
	.cpu_prepare	= smp_spin_table_cpu_prepare,
	.cpu_boot	= smp_spin_table_cpu_boot,
};
