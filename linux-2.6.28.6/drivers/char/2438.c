#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <mach/hardware.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/unistd.h>

#include <plat/gpio-cfg.h>
#include <plat/regs-clock.h>
#include <plat/regs-gpio.h>

#include <plat/gpio-bank-e.h>
#include <plat/gpio-bank-k.h>
#include <plat/gpio-bank-l.h>

#define DEVICE_NAME "2438"

#define TEL_UP 		0
#define TEL_DOWN 	1

static int sbc2440_modem_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned tmp;
	
	switch(cmd) 
	{
		case TEL_DOWN:
			printk("TEL_DOWN\n");

			tmp = readl(S3C64XX_GPLDAT);
			tmp &= ~( 1 << 4 );
			tmp |= ( 1 << 4);
			writel(tmp, S3C64XX_GPLDAT);
			
			return 0;
		case TEL_UP:
			printk("TEL_UP\n");
			
			tmp = readl(S3C64XX_GPLDAT);
			tmp &= ~(1 << 4);
			writel(tmp, S3C64XX_GPLDAT);
		
			return 0;
		default:
			return -EINVAL;
	}
}

static struct file_operations dev_fops = {
	.owner	=	THIS_MODULE,
	.ioctl	=	sbc2440_modem_ioctl,
};

static struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &dev_fops,
};

static int __init modem_init(void)
{
	int 		ret;
	unsigned 	tmp;

	tmp = readl(S3C64XX_GPLCON);
	/* set GPL4 0001 (output)*/
	tmp = (tmp & ~(0xf<<16))|(0x1U<<16);
	writel(tmp, S3C64XX_GPLCON);

	/* set GPL4 low level */
	tmp = readl(S3C64XX_GPLDAT);
	tmp &= ~(1 << 4);
	writel(tmp, S3C64XX_GPLDAT);
		
	ret = misc_register(&misc);

	printk (DEVICE_NAME"\tinitialized\n");

	return ret;
}

static void __exit modem_exit(void)
{
	misc_deregister(&misc);
}

module_init(modem_init);
module_exit(modem_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Handaer Inc.");
