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

#define DEVICE_NAME "spk_switch"


static int s3c64xx_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	
	switch(cmd) 
	{
		case 0:
			printk("DOWN\n");

			return 0;
		case 1:
			printk("UP\n");
			
			return 0;
		default:
			return -EINVAL;
	}
}

static struct file_operations dev_fops = {
	.owner	=	THIS_MODULE,
	.ioctl	=	s3c64xx_ioctl,
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

	printk (DEVICE_NAME"\tinitialized\n");
	tmp = readl(S3C64XX_GPLCON);
	/* set GPL5 0001 (output)*/
	tmp = (tmp & ~( 0xf << 20 )) | (0x1U << 20);
	writel(tmp, S3C64XX_GPLCON);

	tmp = readl(S3C64XX_GPKCON1);
	/* set GPk12 0001 (output)*/
	tmp = (tmp & ~( 0xf << 16 )) | (0x1U << 16);
	writel(tmp, S3C64XX_GPKCON1);

	printk("--------------------------------------------------------------------------------\n");
	printk(KERN_INFO "Control S3C6410 SPK output line:GPLDAT:%x\t,GPKDAT:%x\n", readl(S3C64XX_GPLDAT), readl(S3C64XX_GPKDAT));
	printk("--------------------------------------------------------------------------------\n");
	
	//control GPK12 LOW
	tmp = readl(S3C64XX_GPKDAT);
	tmp &= ~(1 << 12);
	writel(tmp, S3C64XX_GPKDAT);

	//control GPL5 hIgh
	tmp = readl(S3C64XX_GPLDAT);
	tmp |= (0x1 << 5 );
	writel(tmp, S3C64XX_GPLDAT);
	
	printk("S3C6410_GPLDAT:%x\n", readl(S3C64XX_GPLDAT));
	printk("S3C6410_GPKDAT:%x\n", readl(S3C64XX_GPKDAT));

	ret = misc_register(&misc);

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
