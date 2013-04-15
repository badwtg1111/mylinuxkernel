/***********************************************************
 * This is a driver code for 4*5 keyboard
 *
 * 硬件:  		S3C6410
 * 内核版本:  	linux-2.6.28.6
 * 编译工具：	arm-linux-gcc 4.4.1
 * 
 * 4 * 5矩阵键盘
 * 管脚：
 * XEINT0/kpROW0/GPN0	----INPUT
 * XEINT1/kpROW1/GPN1	----INPUT
 * XEINT2/kpROW2/GPN2	----INPUT
 * XEINT3/kpROW3/GPN3	----INPUT
 *
 * XEINT5/kpROW5/GPN5	----OUTPUT
 * XEINT6/kpROW6/GPN6	----OUTPUT
 * XEINT9/GPN9			----OUTPUT
 * XEINT10/GPN10		----OUTPUT
 * XEINT12/GPN12		----OUTPUT
 *
 * DATA : 		2012-8-15
 ***********************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <mach/map.h>
#include <linux/mm.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-clock.h>
#include <plat/regs-gpio.h>
#include <mach/gpio.h>
#include <linux/spinlock.h>
#include <plat/gpio-bank-n.h>

struct input_dev  	*button_devp;
static char 		*name = "s3c6410_keypad";

spinlock_t keypad_lock = SPIN_LOCK_UNLOCKED;

volatile int col_value 	= 0;
volatile int row_value 	= 0;
volatile int key_num 	= 0;

/*struct of irq*/
struct button_irq_desc{
	int 	irq;			//外部中断号
	int 	pin;			//GPIO引脚寄存器
	int 	number;			//引脚号
	char 	*name;			//名称
};

/*IRQ for module*/
static struct button_irq_desc button_irqs[] = {
	{ IRQ_EINT(0),S3C64XX_GPN(0),  0,  "rol1"},
	{ IRQ_EINT(1),S3C64XX_GPN(1),  1,  "rol2"},
	{ IRQ_EINT(2),S3C64XX_GPN(2),  2,  "rol3"},
	{ IRQ_EINT(3),S3C64XX_GPN(3),  3,  "rol4"},
};

void key_delay(void)//按键延时去抖动函数，延时时间为20ms
{
 	unsigned char  i, j;
 	
	for(i=0;i<125;i++)
	  	for(j=0;j<60;j++);
}

/*input_report_key */
static void report_key(int keyno,int report_val)
{
	switch(keyno){
		case 1:
			input_report_key(button_devp, BTN_0, report_val);
			break;
		case 2:
			input_report_key(button_devp, BTN_1, report_val);	
			break;
		case 3:
			input_report_key(button_devp, BTN_2, report_val);
			break;
		case 4:
			input_report_key(button_devp, BTN_3, report_val);
			break;
		case 5:
			input_report_key(button_devp, BTN_4, report_val);    
			break;
		case 6:
			input_report_key(button_devp, BTN_5, report_val);			
			break;
		case 7:
			input_report_key(button_devp, BTN_6, report_val);
			break;
		case 8:
			input_report_key(button_devp, BTN_7, report_val);
			break;
		case 9:
			input_report_key(button_devp, BTN_8, report_val);
			break;
		case 10:
			input_report_key(button_devp, BTN_9, report_val);
			break;
		case 11:
			input_report_key(button_devp, BTN_A, report_val);
			break;
		case 12:
			input_report_key(button_devp, BTN_B, report_val);
			break;
		case 13:
			input_report_key(button_devp, BTN_C, report_val);
			break;
		case 14:
			input_report_key(button_devp, BTN_X, report_val);
			break;
		case 15:
			input_report_key(button_devp, BTN_Y, report_val);
			break;
		case 16:
			input_report_key(button_devp, BTN_Z, report_val);
			break;
		case 17:
			input_report_key(button_devp, BTN_TL, report_val);
			break;
		case 18:
			input_report_key(button_devp, BTN_TR, report_val);
			break;
		case 19:
			input_report_key(button_devp, BTN_TL2, report_val);
			break;
		case 20:
			input_report_key(button_devp, BTN_TR2, report_val);
			break;
		default:
				break;	
	}
	input_sync(button_devp);
}

/*GPIO 的 N port的GPN5， GPN6， GPN9， GPN10， GPN12端口配置*/
static void port1_init(void)
{
	unsigned int tmp = 0;
	
	tmp  = readl(S3C64XX_GPNCON);
	tmp &= ~(0xffffffff);
	tmp |= (1 << 10) | (1 << 12) | (1 << 18) | (1 << 20) | (1 << 24);
	writel(tmp, S3C64XX_GPNCON);
}

/*GPN0,1,2,3端口配置*/
static void port2_init(void)
{
	unsigned int tmp = 0;
	
	tmp  = readl(S3C64XX_GPNCON);
	tmp &= ~(0xffffffff);
	tmp |= (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6);
	writel(tmp,S3C64XX_GPNCON);
}

static void scan_keypad(void)
{ 
	volatile unsigned int temp = 0;
	volatile int i;

 	port1_init();

	temp = readl(S3C64XX_GPNDAT);
	
	if((temp & (1 << 0)) == 0){
		key_delay();	//去延时抖动
		row_value= 1;
	}
	else if((temp & (1 << 1)) == 0){
		key_delay();
		row_value= 2;
	}
	else if((temp & (1 << 2)) == 0){
		key_delay();
		row_value = 3;
	}
	else if((temp & (1 << 3)) == 0)	{
		key_delay();
		row_value = 4;
	}

	for (i=0; i<10000; i++) asm ("");

	port2_init();
	
	temp = readl(S3C64XX_GPNDAT);
	if(temp & (1 << 5)){
		col_value = 1;
	}
	else if(temp & (1 << 6)){
		col_value = 2;
	}
	else if(temp & (1 << 9)){
		col_value = 3;
	}
	else if(temp & (1 << 10)){
		col_value = 4;
	}
	else if(temp & (1 << 12)){
		col_value = 5;
	}
	/*reset GPNDAT 5 6 9 10 12 low level */
	/*The most important step*/
	temp  = readl(S3C64XX_GPNDAT);
	temp &= ~(1 << 5) & ~(1 << 6) & ~(1 << 9) & ~(1 << 10) & ~(1 << 12);
	temp |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
	writel( temp, S3C64XX_GPNDAT);
}

/*According to cval and rval make sure which key... */
static void set_keynum(int cval,int rval,int reportval)
{
	if(rval == 1){
		switch(cval)
		{
			case 1:
				key_num = 1;
				break;
			case 2:
				key_num = 2;
				break;
			case 3:
				key_num = 3;
				break;
			case 4:
				key_num = 4;
				break;
			case 5:
				key_num = 5;
				break;
		}
	}	
	else if(rval == 2){
		switch(cval)
		{
			case 1:
				key_num = 6;
				break;
			case 2:
				key_num = 7;
				break;
			case 3:
				key_num = 8;
				break;
			case 4:
				key_num = 9;
				break;
			case 5:
				key_num = 10;
				break;
		}
	}
	else if(rval == 3){
		switch(cval)
		{
			case 1:
				key_num = 11;
				break;
			case 2:
				key_num = 12;
				break;
			case 3:
				key_num = 13;
				break;
			case 4:
				key_num = 14;
				break;
			case 5:
				key_num = 15;
				break;
		}
	}
	else if(rval == 4){
		switch(cval)
		{
			case 1:
				key_num = 16;
				break;
			case 2:
				key_num = 17;
				break;
			case 3:
				key_num = 18;
				break;
			case 4:
				key_num = 19;
				break;
			case 5:
				key_num = 20;
				break;
		}
	}
}

static int save_irq = 0;  //for updown

/*interrupt code */
static irqreturn_t  button_interrupt(int irq, void* dev_id)
{
	struct button_irq_desc *button_irqs = (struct button_irq_desc*)dev_id;
	unsigned int gpio_value = 0;
	int status, retval;

	disable_irq(IRQ_EINT(0));	
	disable_irq(IRQ_EINT(1));
	disable_irq(IRQ_EINT(2));
	disable_irq(IRQ_EINT(3));
	spin_lock(&keypad_lock);
	
	status = readl(S3C64XX_GPNDAT) & ( 0x1 << button_irqs->number);
	retval = status ? 0 : 1;
	
	if(retval == 1){	
		save_irq = button_irqs->irq;
		
		scan_keypad();
		set_keynum(col_value, row_value, retval);
		report_key(key_num, retval);
	}
	else if( (retval == 0) && (button_irqs->irq == save_irq)){
		report_key(key_num, retval);
	}
	
	gpio_value  = readl(S3C64XX_GPNCON);
	gpio_value &= ~(0xffffffff);
	gpio_value |= ( (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | ( 1 << 10) | (1 << 12) | (1 << 18) | (1 << 20) | (1 << 24) );
	writel(gpio_value, S3C64XX_GPNCON);
	
	spin_unlock(&keypad_lock);
	
	enable_irq(IRQ_EINT(0));	
	enable_irq(IRQ_EINT(1));
	enable_irq(IRQ_EINT(2));
	enable_irq(IRQ_EINT(3));
	
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int request_irq_init(void)
{
	int err = 0;
	int i;
	for(i = 0; i < sizeof(button_irqs)/sizeof(button_irqs[0]); i++)
	{
		err = request_irq(button_irqs[i].irq, button_interrupt, IRQ_TYPE_EDGE_BOTH, button_irqs[i].name, (void *)&button_irqs[i]);
		if(err){
			break;
		}
	}
	if(err){
		i--;
		for(; i >= 0; i--)
		{
			if(button_irqs[i].irq < 0){
				continue;
			}
			disable_irq(button_irqs[i].irq);
			free_irq(button_irqs[i].irq, (void*)&button_irqs[i]);
		}
		return -EBUSY;
	}
	return 0;
}


/*init*/
static int __init button_init(void)
{
	struct input_dev *input_dev;
	int ret;
	
	input_dev = input_allocate_device();
	if(!input_dev){
		printk(KERN_ERR"Unable to alloc the input device!!\n");
	}

	button_devp = input_dev;
	button_devp->name = name;

	request_irq_init();
	
	set_bit(EV_KEY, button_devp->evbit);
	set_bit(BTN_0,	button_devp->keybit);
	set_bit(BTN_1,	button_devp->keybit);
	set_bit(BTN_2,	button_devp->keybit);
	set_bit(BTN_3,	button_devp->keybit);
	set_bit(BTN_4,	button_devp->keybit);
	set_bit(BTN_5,	button_devp->keybit);
	set_bit(BTN_6,	button_devp->keybit);
	set_bit(BTN_7,	button_devp->keybit);
	set_bit(BTN_8,	button_devp->keybit);
	set_bit(BTN_9,	button_devp->keybit);
	set_bit(BTN_A,	button_devp->keybit);
	set_bit(BTN_B,	button_devp->keybit);
	set_bit(BTN_C,	button_devp->keybit);
	set_bit(BTN_X,	button_devp->keybit);
	set_bit(BTN_Y,	button_devp->keybit);
	set_bit(BTN_Z,	button_devp->keybit);
	set_bit(BTN_TL,	button_devp->keybit);
	set_bit(BTN_TR,	button_devp->keybit);
	set_bit(BTN_TL2, button_devp->keybit);
	set_bit(BTN_TR2, button_devp->keybit);

	ret = input_register_device(button_devp);

	if(ret < 0){
		printk("input_register_device(): failed !! \n");
	}

	return ret;
}

static void __exit button_exit(void)
{
	int i;

	for(i = 0; i < sizeof(button_irqs)/sizeof(button_irqs[0]); i++)
	{
		disable_irq(button_irqs[i].irq);
		free_irq(button_irqs[i].irq, (void*)&button_irqs[i]);
	}
	input_unregister_device(button_devp);
}

module_init(button_init);
module_exit(button_exit);

MODULE_DESCRIPTION("Handaer's 4*5 keypad Driver for h1");
MODULE_LICENSE("GPL");
