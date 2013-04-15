/*
 *  SGI Visual Workstation support and quirks, unmaintained.
 *
 *  Split out from setup.c by davej@suse.de
 *
 *	Copyright (C) 1999 Bent Hagemark, Ingo Molnar
 *
 *  SGI Visual Workstation interrupt controller
 *
 *  The Cobalt system ASIC in the Visual Workstation contains a "Cobalt" APIC
 *  which serves as the main interrupt controller in the system.  Non-legacy
 *  hardware in the system uses this controller directly.  Legacy devices
 *  are connected to the PIIX4 which in turn has its 8259(s) connected to
 *  a of the Cobalt APIC entry.
 *
 *  09/02/2000 - Updated for 2.4 by jbarnes@sgi.com
 *
 *  25/11/2002 - Updated for 2.5 by Andrey Panin <pazke@orbita1.ru>
 */
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/visws/cobalt.h>
#include <asm/visws/piix4.h>
#include <asm/arch_hooks.h>
#include <asm/io_apic.h>
#include <asm/fixmap.h>
#include <asm/reboot.h>
#include <asm/setup.h>
#include <asm/e820.h>
#include <asm/io.h>

#include <mach_ipi.h>

#include "mach_apic.h"

#include <linux/kernel_stat.h>

#include <asm/i8259.h>
#include <asm/irq_vectors.h>
#include <asm/visws/lithium.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>

extern int no_broadcast;

#include <asm/apic.h>

char visws_board_type	= -1;
char visws_board_rev	= -1;

int is_visws_box(void)
{
	return visws_board_type >= 0;
}

static int __init visws_time_init(void)
{
	printk(KERN_INFO "Starting Cobalt Timer system clock\n");

	/* Set the countdown value */
	co_cpu_write(CO_CPU_TIMEVAL, CO_TIME_HZ/HZ);

	/* Start the timer */
	co_cpu_write(CO_CPU_CTRL, co_cpu_read(CO_CPU_CTRL) | CO_CTRL_TIMERUN);

	/* Enable (unmask) the timer interrupt */
	co_cpu_write(CO_CPU_CTRL, co_cpu_read(CO_CPU_CTRL) & ~CO_CTRL_TIMEMASK);

	/*
	 * Zero return means the generic timer setup code will set up
	 * the standard vector:
	 */
	return 0;
}

static int __init visws_pre_intr_init(void)
{
	init_VISWS_APIC_irqs();

	/*
	 * We dont want ISA irqs to be set up by the generic code:
	 */
	return 1;
}

/* Quirk for machine specific memory setup. */

#define MB (1024 * 1024)

unsigned long sgivwfb_mem_phys;
unsigned long sgivwfb_mem_size;
EXPORT_SYMBOL(sgivwfb_mem_phys);
EXPORT_SYMBOL(sgivwfb_mem_size);

long long mem_size __initdata = 0;

static char * __init visws_memory_setup(void)
{
	long long gfx_mem_size = 8 * MB;

	mem_size = boot_params.alt_mem_k;

	if (!mem_size) {
		printk(KERN_WARNING "Bootloader didn't set memory size, upgrade it !\n");
		mem_size = 128 * MB;
	}

	/*
	 * this hardcodes the graphics memory to 8 MB
	 * it really should be sized dynamically (or at least
	 * set as a boot param)
	 */
	if (!sgivwfb_mem_size) {
		printk(KERN_WARNING "Defaulting to 8 MB framebuffer size\n");
		sgivwfb_mem_size = 8 * MB;
	}

	/*
	 * Trim to nearest MB
	 */
	sgivwfb_mem_size &= ~((1 << 20) - 1);
	sgivwfb_mem_phys = mem_size - gfx_mem_size;

	e820_add_region(0, LOWMEMSIZE(), E820_RAM);
	e820_add_region(HIGH_MEMORY, mem_size - sgivwfb_mem_size - HIGH_MEMORY, E820_RAM);
	e820_add_region(sgivwfb_mem_phys, sgivwfb_mem_size, E820_RESERVED);

	return "PROM";
}

static void visws_machine_emergency_restart(void)
{
	/*
	 * Visual Workstations restart after this
	 * register is poked on the PIIX4
	 */
	outb(PIIX4_RESET_VAL, PIIX4_RESET_PORT);
}

static void visws_machine_power_off(void)
{
	unsigned short pm_status;
/*	extern unsigned int pci_bus0; */

	while ((pm_status = inw(PMSTS_PORT)) & 0x100)
		outw(pm_status, PMSTS_PORT);

	outw(PM_SUSPEND_ENABLE, PMCNTRL_PORT);

	mdelay(10);

#define PCI_CONF1_ADDRESS(bus, devfn, reg) \
	(0x80000000 | (bus << 16) | (devfn << 8) | (reg & ~3))

/*	outl(PCI_CONF1_ADDRESS(pci_bus0, SPECIAL_DEV, SPECIAL_REG), 0xCF8); */
	outl(PIIX_SPECIAL_STOP, 0xCFC);
}

static int __init visws_get_smp_config(unsigned int early)
{
	/*
	 * Prevent MP-table parsing by the generic code:
	 */
	return 1;
}

/*
 * The Visual Workstation is Intel MP compliant in the hardware
 * sense, but it doesn't have a BIOS(-configuration table).
 * No problem for Linux.
 */

static void __init MP_processor_info(struct mpc_config_processor *m)
{
	int ver, logical_apicid;
	physid_mask_t apic_cpus;

	if (!(m->mpc_cpuflag & CPU_ENABLED))
		return;

	logical_apicid = m->mpc_apicid;
	printk(KERN_INFO "%sCPU #%d %u:%u APIC version %d\n",
	       m->mpc_cpuflag & CPU_BOOTPROCESSOR ? "Bootup " : "",
	       m->mpc_apicid,
	       (m->mpc_cpufeature & CPU_FAMILY_MASK) >> 8,
	       (m->mpc_cpufeature & CPU_MODEL_MASK) >> 4,
	       m->mpc_apicver);

	if (m->mpc_cpuflag & CPU_BOOTPROCESSOR)
		boot_cpu_physical_apicid = m->mpc_apicid;

	ver = m->mpc_apicver;
	if ((ver >= 0x14 && m->mpc_apicid >= 0xff) || m->mpc_apicid >= 0xf) {
		printk(KERN_ERR "Processor #%d INVALID. (Max ID: %d).\n",
			m->mpc_apicid, MAX_APICS);
		return;
	}

	apic_cpus = apicid_to_cpu_present(m->mpc_apicid);
	physids_or(phys_cpu_present_map, phys_cpu_present_map, apic_cpus);
	/*
	 * Validate version
	 */
	if (ver == 0x0) {
		printk(KERN_ERR "BIOS bug, APIC version is 0 for CPU#%d! "
			"fixing up to 0x10. (tell your hw vendor)\n",
			m->mpc_apicid);
		ver = 0x10;
	}
	apic_version[m->mpc_apicid] = ver;
}

static int __init visws_find_smp_config(unsigned int reserve)
{
	struct mpc_config_processor *mp = phys_to_virt(CO_CPU_TAB_PHYS);
	unsigned short ncpus = readw(phys_to_virt(CO_CPU_NUM_PHYS));

	if (ncpus > CO_CPU_MAX) {
		printk(KERN_WARNING "find_visws_smp: got cpu count of %d at %p\n",
			ncpus, mp);

		ncpus = CO_CPU_MAX;
	}

	if (ncpus > setup_max_cpus)
		ncpus = setup_max_cpus;

#ifdef CONFIG_X86_LOCAL_APIC
	smp_found_config = 1;
#endif
	while (ncpus--)
		MP_processor_info(mp++);

	mp_lapic_addr = APIC_DEFAULT_PHYS_BASE;

	return 1;
}

static int visws_trap_init(void);

static struct x86_quirks visws_x86_quirks __initdata = {
	.arch_time_init		= visws_time_init,
	.arch_pre_intr_init	= visws_pre_intr_init,
	.arch_memory_setup	= visws_memory_setup,
	.arch_intr_init		= NULL,
	.arch_trap_init		= visws_trap_init,
	.mach_get_smp_config	= visws_get_smp_config,
	.mach_find_smp_config	= visws_find_smp_config,
};

void __init visws_early_detect(void)
{
	int raw;

	visws_board_type = (char)(inb_p(PIIX_GPI_BD_REG) & PIIX_GPI_BD_REG)
							 >> PIIX_GPI_BD_SHIFT;

	if (visws_board_type < 0)
		return;

	/*
	 * Install special quirks for timer, interrupt and memory setup:
	 * Fall back to generic behavior for traps:
	 * Override generic MP-table parsing:
	 */
	x86_quirks = &visws_x86_quirks;

	/*
	 * Install reboot quirks:
	 */
	pm_power_off			= visws_machine_power_off;
	machine_ops.emergency_restart	= visws_machine_emergency_restart;

	/*
	 * Do not use broadcast IPIs:
	 */
	no_broadcast = 0;

#ifdef CONFIG_X86_IO_APIC
	/*
	 * Turn off IO-APIC detection and initialization:
	 */
	skip_ioapic_setup		= 1;
#endif

	/*
	 * Get Board rev.
	 * First, we have to initialize the 307 part to allow us access
	 * to the GPIO registers.  Let's map them at 0x0fc0 which is right
	 * after the PIIX4 PM section.
	 */
	outb_p(SIO_DEV_SEL, SIO_INDEX);
	outb_p(SIO_GP_DEV, SIO_DATA);	/* Talk to GPIO regs. */

	outb_p(SIO_DEV_MSB, SIO_INDEX);
	outb_p(SIO_GP_MSB, SIO_DATA);	/* MSB of GPIO base address */

	outb_p(SIO_DEV_LSB, SIO_INDEX);
	outb_p(SIO_GP_LSB, SIO_DATA);	/* LSB of GPIO base address */

	outb_p(SIO_DEV_ENB, SIO_INDEX);
	outb_p(1, SIO_DATA);		/* Enable GPIO registers. */

	/*
	 * Now, we have to map the power management section to write
	 * a bit which enables access to the GPIO registers.
	 * What lunatic came up with this shit?
	 */
	outb_p(SIO_DEV_SEL, SIO_INDEX);
	outb_p(SIO_PM_DEV, SIO_DATA);	/* Talk to GPIO regs. */

	outb_p(SIO_DEV_MSB, SIO_INDEX);
	outb_p(SIO_PM_MSB, SIO_DATA);	/* MSB of PM base address */

	outb_p(SIO_DEV_LSB, SIO_INDEX);
	outb_p(SIO_PM_LSB, SIO_DATA);	/* LSB of PM base address */

	outb_p(SIO_DEV_ENB, SIO_INDEX);
	outb_p(1, SIO_DATA);		/* Enable PM registers. */

	/*
	 * Now, write the PM register which enables the GPIO registers.
	 */
	outb_p(SIO_PM_FER2, SIO_PM_INDEX);
	outb_p(SIO_PM_GP_EN, SIO_PM_DATA);

	/*
	 * Now, initialize the GPIO registers.
	 * We want them all to be inputs which is the
	 * power on default, so let's leave them alone.
	 * So, let's just read the board rev!
	 */
	raw = inb_p(SIO_GP_DATA1);
	raw &= 0x7f;	/* 7 bits of valid board revision ID. */

	if (visws_board_type == VISWS_320) {
		if (raw < 0x6) {
			visws_board_rev = 4;
		} else if (raw < 0xc) {
			visws_board_rev = 5;
		} else {
			visws_board_rev = 6;
		}
	} else if (visws_board_type == VISWS_540) {
			visws_board_rev = 2;
		} else {
			visws_board_rev = raw;
		}

	printk(KERN_INFO "Silicon Graphics Visual Workstation %s (rev %d) detected\n",
	       (visws_board_type == VISWS_320 ? "320" :
	       (visws_board_type == VISWS_540 ? "540" :
		"unknown")), visws_board_rev);
}

#define A01234 (LI_INTA_0 | LI_INTA_1 | LI_INTA_2 | LI_INTA_3 | LI_INTA_4)
#define BCD (LI_INTB | LI_INTC | LI_INTD)
#define ALLDEVS (A01234 | BCD)

static __init void lithium_init(void)
{
	set_fixmap(FIX_LI_PCIA, LI_PCI_A_PHYS);
	set_fixmap(FIX_LI_PCIB, LI_PCI_B_PHYS);

	if ((li_pcia_read16(PCI_VENDOR_ID) != PCI_VENDOR_ID_SGI) ||
	    (li_pcia_read16(PCI_DEVICE_ID) != PCI_DEVICE_ID_SGI_LITHIUM)) {
		printk(KERN_EMERG "Lithium hostbridge %c not found\n", 'A');
/*		panic("This machine is not SGI Visual Workstation 320/540"); */
	}

	if ((li_pcib_read16(PCI_VENDOR_ID) != PCI_VENDOR_ID_SGI) ||
	    (li_pcib_read16(PCI_DEVICE_ID) != PCI_DEVICE_ID_SGI_LITHIUM)) {
		printk(KERN_EMERG "Lithium hostbridge %c not found\n", 'B');
/*		panic("This machine is not SGI Visual Workstation 320/540"); */
	}

	li_pcia_write16(LI_PCI_INTEN, ALLDEVS);
	li_pcib_write16(LI_PCI_INTEN, ALLDEVS);
}

static __init void cobalt_init(void)
{
	/*
	 * On normal SMP PC this is used only with SMP, but we have to
	 * use it and set it up here to start the Cobalt clock
	 */
	set_fixmap(FIX_APIC_BASE, APIC_DEFAULT_PHYS_BASE);
	setup_local_APIC();
	printk(KERN_INFO "Local APIC Version %#x, ID %#x\n",
		(unsigned int)apic_read(APIC_LVR),
		(unsigned int)apic_read(APIC_ID));

	set_fixmap(FIX_CO_CPU, CO_CPU_PHYS);
	set_fixmap(FIX_CO_APIC, CO_APIC_PHYS);
	printk(KERN_INFO "Cobalt Revision %#lx, APIC ID %#lx\n",
		co_cpu_read(CO_CPU_REV), co_apic_read(CO_APIC_ID));

	/* Enable Cobalt APIC being careful to NOT change the ID! */
	co_apic_write(CO_APIC_ID, co_apic_read(CO_APIC_ID) | CO_APIC_ENABLE);

	printk(KERN_INFO "Cobalt APIC enabled: ID reg %#lx\n",
		co_apic_read(CO_APIC_ID));
}

static int __init visws_trap_init(void)
{
	lithium_init();
	cobalt_init();

	return 1;
}

/*
 * IRQ controller / APIC support:
 */

static DEFINE_SPINLOCK(cobalt_lock);

/*
 * Set the given Cobalt APIC Redirection Table entry to point
 * to the given IDT vector/index.
 */
static inline void co_apic_set(int entry, int irq)
{
	co_apic_write(CO_APIC_LO(entry), CO_APIC_LEVEL | (irq + FIRST_EXTERNAL_VECTOR));
	co_apic_write(CO_APIC_HI(entry), 0);
}

/*
 * Cobalt (IO)-APIC functions to handle PCI devices.
 */
static inline int co_apic_ide0_hack(void)
{
	extern char visws_board_type;
	extern char visws_board_rev;

	if (visws_board_type == VISWS_320 && visws_board_rev == 5)
		return 5;
	return CO_APIC_IDE0;
}

static int is_co_apic(unsigned int irq)
{
	if (IS_CO_APIC(irq))
		return CO_APIC(irq);

	switch (irq) {
		case 0: return CO_APIC_CPU;
		case CO_IRQ_IDE0: return co_apic_ide0_hack();
		case CO_IRQ_IDE1: return CO_APIC_IDE1;
		default: return -1;
	}
}


/*
 * This is the SGI Cobalt (IO-)APIC:
 */

static void enable_cobalt_irq(unsigned int irq)
{
	co_apic_set(is_co_apic(irq), irq);
}

static void disable_cobalt_irq(unsigned int irq)
{
	int entry = is_co_apic(irq);

	co_apic_write(CO_APIC_LO(entry), CO_APIC_MASK);
	co_apic_read(CO_APIC_LO(entry));
}

/*
 * "irq" really just serves to identify the device.  Here is where we
 * map this to the Cobalt APIC entry where it's physically wired.
 * This is called via request_irq -> setup_irq -> irq_desc->startup()
 */
static unsigned int startup_cobalt_irq(unsigned int irq)
{
	unsigned long flags;
	struct irq_desc *desc = irq_to_desc(irq);

	spin_lock_irqsave(&cobalt_lock, flags);
	if ((desc->status & (IRQ_DISABLED | IRQ_INPROGRESS | IRQ_WAITING)))
		desc->status &= ~(IRQ_DISABLED | IRQ_INPROGRESS | IRQ_WAITING);
	enable_cobalt_irq(irq);
	spin_unlock_irqrestore(&cobalt_lock, flags);
	return 0;
}

static void ack_cobalt_irq(unsigned int irq)
{
	unsigned long flags;

	spin_lock_irqsave(&cobalt_lock, flags);
	disable_cobalt_irq(irq);
	apic_write(APIC_EOI, APIC_EIO_ACK);
	spin_unlock_irqrestore(&cobalt_lock, flags);
}

static void end_cobalt_irq(unsigned int irq)
{
	unsigned long flags;
	struct irq_desc *desc = irq_to_desc(irq);

	spin_lock_irqsave(&cobalt_lock, flags);
	if (!(desc->status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_cobalt_irq(irq);
	spin_unlock_irqrestore(&cobalt_lock, flags);
}

static struct irq_chip cobalt_irq_type = {
	.typename =	"Cobalt-APIC",
	.startup =	startup_cobalt_irq,
	.shutdown =	disable_cobalt_irq,
	.enable =	enable_cobalt_irq,
	.disable =	disable_cobalt_irq,
	.ack =		ack_cobalt_irq,
	.end =		end_cobalt_irq,
};


/*
 * This is the PIIX4-based 8259 that is wired up indirectly to Cobalt
 * -- not the manner expected by the code in i8259.c.
 *
 * there is a 'master' physical interrupt source that gets sent to
 * the CPU. But in the chipset there are various 'virtual' interrupts
 * waiting to be handled. We represent this to Linux through a 'master'
 * interrupt controller type, and through a special virtual interrupt-
 * controller. Device drivers only see the virtual interrupt sources.
 */
static unsigned int startup_piix4_master_irq(unsigned int irq)
{
	init_8259A(0);

	return startup_cobalt_irq(irq);
}

static void end_piix4_master_irq(unsigned int irq)
{
	unsigned long flags;

	spin_lock_irqsave(&cobalt_lock, flags);
	enable_cobalt_irq(irq);
	spin_unlock_irqrestore(&cobalt_lock, flags);
}

static struct irq_chip piix4_master_irq_type = {
	.typename =	"PIIX4-master",
	.startup =	startup_piix4_master_irq,
	.ack =		ack_cobalt_irq,
	.end =		end_piix4_master_irq,
};


static struct irq_chip piix4_virtual_irq_type = {
	.typename =	"PIIX4-virtual",
	.shutdown =	disable_8259A_irq,
	.enable =	enable_8259A_irq,
	.disable =	disable_8259A_irq,
};


/*
 * PIIX4-8259 master/virtual functions to handle interrupt requests
 * from legacy devices: floppy, parallel, serial, rtc.
 *
 * None of these get Cobalt APIC entries, neither do they have IDT
 * entries. These interrupts are purely virtual and distributed from
 * the 'master' interrupt source: CO_IRQ_8259.
 *
 * When the 8259 interrupts its handler figures out which of these
 * devices is interrupting and dispatches to its handler.
 *
 * CAREFUL: devices see the 'virtual' interrupt only. Thus disable/
 * enable_irq gets the right irq. This 'master' irq is never directly
 * manipulated by any driver.
 */
static irqreturn_t piix4_master_intr(int irq, void *dev_id)
{
	int realirq;
	irq_desc_t *desc;
	unsigned long flags;

	spin_lock_irqsave(&i8259A_lock, flags);

	/* Find out what's interrupting in the PIIX4 master 8259 */
	outb(0x0c, 0x20);		/* OCW3 Poll command */
	realirq = inb(0x20);

	/*
	 * Bit 7 == 0 means invalid/spurious
	 */
	if (unlikely(!(realirq & 0x80)))
		goto out_unlock;

	realirq &= 7;

	if (unlikely(realirq == 2)) {
		outb(0x0c, 0xa0);
		realirq = inb(0xa0);

		if (unlikely(!(realirq & 0x80)))
			goto out_unlock;

		realirq = (realirq & 7) + 8;
	}

	/* mask and ack interrupt */
	cached_irq_mask |= 1 << realirq;
	if (unlikely(realirq > 7)) {
		inb(0xa1);
		outb(cached_slave_mask, 0xa1);
		outb(0x60 + (realirq & 7), 0xa0);
		outb(0x60 + 2, 0x20);
	} else {
		inb(0x21);
		outb(cached_master_mask, 0x21);
		outb(0x60 + realirq, 0x20);
	}

	spin_unlock_irqrestore(&i8259A_lock, flags);

	desc = irq_to_desc(realirq);

	/*
	 * handle this 'virtual interrupt' as a Cobalt one now.
	 */
	kstat_incr_irqs_this_cpu(realirq, desc);

	if (likely(desc->action != NULL))
		handle_IRQ_event(realirq, desc->action);

	if (!(desc->status & IRQ_DISABLED))
		enable_8259A_irq(realirq);

	return IRQ_HANDLED;

out_unlock:
	spin_unlock_irqrestore(&i8259A_lock, flags);
	return IRQ_NONE;
}

static struct irqaction master_action = {
	.handler =	piix4_master_intr,
	.name =		"PIIX4-8259",
};

static struct irqaction cascade_action = {
	.handler = 	no_action,
	.name =		"cascade",
};


void init_VISWS_APIC_irqs(void)
{
	int i;

	for (i = 0; i < CO_IRQ_APIC0 + CO_APIC_LAST + 1; i++) {
		struct irq_desc *desc = irq_to_desc(i);

		desc->status = IRQ_DISABLED;
		desc->action = 0;
		desc->depth = 1;

		if (i == 0) {
			desc->chip = &cobalt_irq_type;
		}
		else if (i == CO_IRQ_IDE0) {
			desc->chip = &cobalt_irq_type;
		}
		else if (i == CO_IRQ_IDE1) {
			desc->chip = &cobalt_irq_type;
		}
		else if (i == CO_IRQ_8259) {
			desc->chip = &piix4_master_irq_type;
		}
		else if (i < CO_IRQ_APIC0) {
			desc->chip = &piix4_virtual_irq_type;
		}
		else if (IS_CO_APIC(i)) {
			desc->chip = &cobalt_irq_type;
		}
	}

	setup_irq(CO_IRQ_8259, &master_action);
	setup_irq(2, &cascade_action);
}
