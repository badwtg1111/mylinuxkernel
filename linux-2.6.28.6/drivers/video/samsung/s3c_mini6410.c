/*
 * drivers/video/s3c/s3cfb_mini6410.c
 *
 * based on s3cfb_lte480wv.c
 *
 * Copyright (C) 2008 Jinsung Yang <jsgood.yang@samsung.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	S3C Frame Buffer Driver
 *	based on skeletonfb.c, sa1100fb.h, s3c2410fb.c
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <plat/regs-gpio.h>
#include <plat/regs-lcd.h>

#include "s3cfb.h"
#include <linux/autoconf.h>

#if defined(CONFIG_FB_S3C_TFT480272)

#define S3CFB_VBP		(1)	/* back porch */
#define S3CFB_VFP		(1)	/* front porch */
#define S3CFB_VSW		(1)	/* vsync width */
#define S3CFB_HBP		(0x02)	/* back porch */
#define S3CFB_HFP		(0x03)	/* front porch */
#define S3CFB_HSW		(0x28)	/* hsync width */

#define S3CFB_HRES		480	/* horizon pixel  x resolition */
#define S3CFB_VRES		272	/* line cnt       y resolution */

#define S3CFB_CLKVAL		9

#elif defined(CONFIG_FB_S3C_TFT800480)

#define S3CFB_VBP		(0x02)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x04)	/* vsync width */
#define S3CFB_HBP		(0x02)	/* back porch */
#define S3CFB_HFP		(0x02)	/* front porch */
#define S3CFB_HSW		(0x29)	/* hsync width */

#define S3CFB_HRES		800	/* horizon pixel  x resolition */
#define S3CFB_VRES		480	/* line cnt       y resolution */

#define S3CFB_CLKVAL		3 //fixed by suxiaozhi 20111128

#elif defined(CONFIG_FB_S3C_T240320)

#define S3CFB_VBP		(0x01)	/* back porch */
#define S3CFB_VFP		(0x01)	/* front porch */
#define S3CFB_VSW		(0x04)	/* vsync width */
#define S3CFB_HBP		(0x01)	/* back porch */
#define S3CFB_HFP		(0x04)	/* front porch */
#define S3CFB_HSW		(0x1E)	/* hsync width */

#define S3CFB_HRES		240	/* horizon pixel  x resolition */
#define S3CFB_VRES		320	/* line cnt       y resolution */

#define S3CFB_CLKVAL		11

#elif defined(CONFIG_FB_S3C_TFT640480)

#define S3CFB_VBP		(0x01)	/* back porch */
#define S3CFB_VFP		(0x01)	/* front porch */
#define S3CFB_VSW		(0x01)	/* vsync width */
#define S3CFB_HBP		(0x03)	/* back porch */
#define S3CFB_HFP		(0x03)	/* front porch */
#define S3CFB_HSW		(0x28)	/* hsync width */

#define S3CFB_HRES		640	/* horizon pixel  x resolition */
#define S3CFB_VRES		480	/* line cnt       y resolution */

#define S3CFB_CLKVAL		3

#elif defined(CONFIG_FB_S3C_VGA1024768)

#define S3CFB_VBP		(0x02)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x10)	/* vsync width */
#define S3CFB_HBP		(0x02)	/* back porch */
#define S3CFB_HFP		(0x02)	/* front porch */
#define S3CFB_HSW		(0x2A)	/* hsync width */

#define S3CFB_HRES		1024	/* horizon pixel  x resolition */
#define S3CFB_VRES		768	/* line cnt       y resolution */

#define S3CFB_CLKVAL		5

#elif defined(CONFIG_FB_S3C_VGA800600)

#define S3CFB_VBP		(0x02)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x10)	/* vsync width */
#define S3CFB_HBP		(0x02)	/* back porch */
#define S3CFB_HFP		(0x02)	/* front porch */
#define S3CFB_HSW		(0x2A)	/* hsync width */

#define S3CFB_HRES		800	/* horizon pixel  x resolition */
#define S3CFB_VRES		600	/* line cnt       y resolution */

#define S3CFB_CLKVAL		5

#elif defined(CONFIG_FB_S3C_VGA640480)

#define S3CFB_VBP		(0x02)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x10)	/* vsync width */
#define S3CFB_HBP		(0x02)	/* back porch */
#define S3CFB_HFP		(0x02)	/* front porch */
#define S3CFB_HSW		(0x2A)	/* hsync width */

#define S3CFB_HRES		640	/* horizon pixel  x resolition */
#define S3CFB_VRES		480	/* line cnt       y resolution */

#define S3CFB_CLKVAL		5

#elif defined(CONFIG_FB_S3C_EZVGA800600)

#define S3CFB_VBP		(0x02)	/* back porch */
#define S3CFB_VFP		(0x02)	/* front porch */
#define S3CFB_VSW		(0x10)	/* vsync width */
#define S3CFB_HBP		(0xA8)	/* back porch */
#define S3CFB_HFP		(0x11)	/* front porch */
#define S3CFB_HSW		(0x2A)	/* hsync width */

#define S3CFB_HRES		800	/* horizon pixel  x resolition */
#define S3CFB_VRES		600	/* line cnt       y resolution */

#define S3CFB_CLKVAL		2

#else
#error mini2440 frame buffer driver not configed
#endif

#define S3CFB_HRES_VIRTUAL	S3CFB_HRES	/* horizon pixel  x resolition */
#define S3CFB_VRES_VIRTUAL	S3CFB_VRES	/* line cnt       y resolution */

#define S3CFB_HRES_OSD		S3CFB_HRES	/* horizon pixel  x resolition */
#define S3CFB_VRES_OSD		S3CFB_VRES	/* line cnt       y resolution */

#define S3CFB_PIXEL_CLOCK	S3CFB_CLKVAL

static void s3cfb_set_fimd_info(void)
{
	s3cfb_fimd.vidcon1 = S3C_VIDCON1_IHSYNC_INVERT | S3C_VIDCON1_IVSYNC_INVERT | S3C_VIDCON1_IVDEN_NORMAL;

#if defined(CONFIG_FB_S3C_VGA1024768) || defined(CONFIG_FB_S3C_VGA800600) || defined(CONFIG_FB_S3C_VGA640480) 
	s3cfb_fimd.vidcon1 = 0;
#endif
	s3cfb_fimd.vidtcon0 = S3C_VIDTCON0_VBPD(S3CFB_VBP - 1) | S3C_VIDTCON0_VFPD(S3CFB_VFP - 1) | S3C_VIDTCON0_VSPW(S3CFB_VSW - 1);
	s3cfb_fimd.vidtcon1 = S3C_VIDTCON1_HBPD(S3CFB_HBP - 1) | S3C_VIDTCON1_HFPD(S3CFB_HFP - 1) | S3C_VIDTCON1_HSPW(S3CFB_HSW - 1);
	s3cfb_fimd.vidtcon2 = S3C_VIDTCON2_LINEVAL(S3CFB_VRES - 1) | S3C_VIDTCON2_HOZVAL(S3CFB_HRES - 1);

	s3cfb_fimd.vidosd0a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3cfb_fimd.vidosd0b = S3C_VIDOSDxB_OSD_RBX_F(S3CFB_HRES - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3CFB_VRES - 1);

	s3cfb_fimd.vidosd1a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3cfb_fimd.vidosd1b = S3C_VIDOSDxB_OSD_RBX_F(S3CFB_HRES_OSD - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3CFB_VRES_OSD - 1);

	s3cfb_fimd.width = S3CFB_HRES;
	s3cfb_fimd.height = S3CFB_VRES;
	s3cfb_fimd.xres = S3CFB_HRES;
	s3cfb_fimd.yres = S3CFB_VRES;

#if defined(CONFIG_FB_S3C_VIRTUAL_SCREEN)
	s3cfb_fimd.xres_virtual = S3CFB_HRES_VIRTUAL;
	s3cfb_fimd.yres_virtual = S3CFB_VRES_VIRTUAL;
#else
	s3cfb_fimd.xres_virtual = S3CFB_HRES;
	s3cfb_fimd.yres_virtual = S3CFB_VRES;
#endif

	s3cfb_fimd.osd_width = S3CFB_HRES_OSD;
	s3cfb_fimd.osd_height = S3CFB_VRES_OSD;
	s3cfb_fimd.osd_xres = S3CFB_HRES_OSD;
	s3cfb_fimd.osd_yres = S3CFB_VRES_OSD;

	s3cfb_fimd.osd_xres_virtual = S3CFB_HRES_OSD;
	s3cfb_fimd.osd_yres_virtual = S3CFB_VRES_OSD;

	s3cfb_fimd.pixclock = S3CFB_PIXEL_CLOCK;

	s3cfb_fimd.hsync_len = S3CFB_HSW;
	s3cfb_fimd.vsync_len = S3CFB_VSW;
	s3cfb_fimd.left_margin = S3CFB_HFP;
	s3cfb_fimd.upper_margin = S3CFB_VFP;
	s3cfb_fimd.right_margin = S3CFB_HBP;
	s3cfb_fimd.lower_margin = S3CFB_VBP;
}

void s3cfb_init_hw(void)
{
	printk(KERN_INFO "LCD TYPE :: LTE480WV will be initialized\n");

	s3cfb_set_fimd_info();
	s3cfb_set_gpio();
}

