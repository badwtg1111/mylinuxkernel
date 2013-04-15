#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <mach/gpio-core.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-clock.h>
#include <plat/regs-gpio.h>

#include <plat/gpio-bank-o.h>

#define POWER_ON_OFF S3C64XX_GPO(8)
#define POWER_DET	S3C64XX_GPN(11)
#define IRQ_POWER_ON_OFF IRQ_EINT(11)

static int is_power_on = 0;
static irqreturn_t power_off_int(int irq, void *data)
{
	int status;	
	//writel(readl(S3C64XX_EINT0MASK)|(1<<11),S3C64XX_EINT0MASK); 		  //disable EINT0 Interrupt before deal it
	//writel(1<<11,S3C64XX_EINT0PEND);									//disable Interrupt
	//writel(readl(S3C64XX_EINT0PEND)&(~(1<<11)),S3C64XX_EINT0PEND);

	/* 判断是按下还是放开中断 */
	status = gpio_get_value(POWER_DET);
	if (status)
	{		
		//printk(KERN_INFO "power key up!!!\n");
		/* 按键释放 */
		/* 标记系统已经上电 */
		is_power_on = 1;
	}
	else
	{
		//printk(KERN_INFO "power key down!!!\n");
		/* 按键按下 */
		if (is_power_on)		/* 如果系统已经上电，此时按下按键，就断电关机 */
		{
			/* 拉低POWER_ON_OFF，使系统断电 */
			gpio_set_value(POWER_ON_OFF, 0);
		//	printk(KERN_INFO "POWER_ON_OFF = [%d]!!!\n", gpio_get_value(POWER_ON_OFF));
			if( gpio_get_value(POWER_ON_OFF) == 1)
			{
				printk(KERN_ERR "can't set POWER_ON_OFF = 0\n");
			}
			is_power_on = 0;
		}
	}
	//writel(readl(S3C64XX_EINT0MASK)&(~(1<<11)),S3C64XX_EINT0MASK);		//Enable Interrupt after deal it
	return IRQ_HANDLED;
}

static int __init power_on_init(void)
{	
	printk(KERN_INFO "power_on_init!!!\n");
	int ret;
	if( (gpio_request(POWER_ON_OFF, "POWER_ON_OFF") == 0) &&
		(gpio_direction_output(POWER_ON_OFF, 1) == 0))
	{
		gpio_export(POWER_ON_OFF, 0);	
		//printk(KERN_INFO "POWER_ON_OFF = [%d]!!!\n", gpio_get_value(POWER_ON_OFF));
		gpio_set_value(POWER_ON_OFF, 1);
		if( gpio_get_value(POWER_ON_OFF) == 0)
		{
			printk(KERN_ERR "can't set POWER_ON_OFF = 1\n");
		}
	}
	else{
		printk(KERN_ERR "cluld not obtain gpio for 'POWER_ON_OFF'\n");
	}
//	printk(KERN_INFO "POWER_ON_OFF = [%d]!!!\n", gpio_get_value(POWER_ON_OFF));

	/* set power_det interrupt */
	s3c_gpio_cfgpin(POWER_DET, S3C_GPIO_SFN(2));
	/* set power_det pull none */
	s3c_gpio_setpull(POWER_DET, S3C_GPIO_PULL_NONE);
	/* set power_det irq both */
	set_irq_type(POWER_DET, IRQ_TYPE_EDGE_BOTH);

	ret = request_irq(IRQ_POWER_ON_OFF, power_off_int, IRQF_DISABLED | IRQ_TYPE_EDGE_BOTH, "power_on_off", NULL);
	if(ret < 0)
	{
		printk(KERN_ERR "request_irq IRQ_POWER_ON_OFF failed !!!\n");
	}
	else if(ret == 0)
	{
		printk(KERN_ERR "request_irq IRQ_POWER_ON_OFF successed !!!\n");
	}
	return 0;
}

static void __exit power_on_exit(void)
{
	free_irq(IRQ_POWER_ON_OFF, NULL);
	printk(KERN_INFO "power_on_exit!!!\n");
}


module_init(power_on_init);
module_exit(power_on_exit);

MODULE_DESCRIPTION("Handaer 6410Base power on/off control!");
MODULE_LICENSE("GPL");

