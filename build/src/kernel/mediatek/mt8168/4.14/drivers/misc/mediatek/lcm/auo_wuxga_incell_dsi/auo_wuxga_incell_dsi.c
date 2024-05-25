/* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein
* is confidential and proprietary to MediaTek Inc. and/or its licensors.
* Without the prior written permission of MediaTek inc. and/or its licensors,
* any reproduction, modification, use or disclosure of MediaTek Software,
* and information contained herein, in whole or in part, shall be strictly prohibited.
*/
/* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
* AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
* NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
* SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
* SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
* THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
* THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
* CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
* SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
* CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
* AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
* OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
* MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
* Copyright (C) 2019 SANYO Techno Solutions Tottori Co., Ltd.
*
*/

#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/i2c.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#endif

#include "lcm_drv.h"

static struct regulator *lcm_vgp;

/***********************************************/
/***********************************************/

static unsigned int GPIO_LCD_PWR;
static unsigned int GPIO_LCD_PWR2;
static unsigned int GPIO_LCD_BL_EN;
static unsigned int GPIO_LCD_RST;

/* get LDO supply */
static int lcm_get_vgp_supply(struct device *dev)
{
	int ret;
	struct regulator *lcm_vgp_ldo;

	pr_info("LCM: %s enter\n", __func__);

	lcm_vgp_ldo = devm_regulator_get(dev, "reg-lcm");
	if (IS_ERR(lcm_vgp_ldo)) {
		ret = PTR_ERR(lcm_vgp_ldo);
		pr_info("failed to get reg-lcm LDO, %d\n", ret);
		return ret;
	}

	pr_info("LCM: lcm get supply ok.\n");

	/* get current voltage settings */
	ret = regulator_get_voltage(lcm_vgp_ldo);
	pr_info("lcm LDO voltage = %d in LK stage\n", ret);

	lcm_vgp = lcm_vgp_ldo;

	return ret;
}

int lcm_vgp_supply_enable(void)
{
	int ret;
	unsigned int volt;

	pr_info("LCM: %s enter\n", __func__);

	if (lcm_vgp == NULL)
		return 0;

	pr_info("LCM: set regulator voltage lcm_vgp voltage to 1.8V\n");
	/* set voltage to 1.8V */
	ret = regulator_set_voltage(lcm_vgp, 1800000, 1800000);
	if (ret != 0) {
		pr_info("LCM: lcm failed to set lcm_vgp voltage: %d\n", ret);
		return ret;
	}

	/* get voltage settings again */
	volt = regulator_get_voltage(lcm_vgp);
	if (volt == 1800000)
		pr_info("LCM: check regulator voltage=1800000 pass!\n");
	else
		pr_info("LCM: check regulator voltage=1800000 fail! (voltage: %d)\n",
			volt);

	ret = regulator_enable(lcm_vgp);
	if (ret != 0) {
		pr_info("LCM: Failed to enable lcm_vgp: %d\n", ret);
		return ret;
	}

	return ret;
}

int lcm_vgp_supply_disable(void)
{
	int ret = 0;
	unsigned int isenable;

	if (lcm_vgp == NULL)
		return 0;

	/* disable regulator */
	isenable = regulator_is_enabled(lcm_vgp);

	pr_info("LCM: lcm query regulator enable status[%d]\n", isenable);

	if (isenable) {
		ret = regulator_disable(lcm_vgp);
		if (ret != 0) {
			pr_info("LCM: lcm failed to disable lcm_vgp: %d\n",
				ret);
			return ret;
		}
		/* verify */
		isenable = regulator_is_enabled(lcm_vgp);
		if (!isenable)
			pr_info("LCM: lcm regulator disable pass\n");
	}

	return ret;
}

static int lcm_get_gpio(struct device *dev)
{
	GPIO_LCD_PWR = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr", 0);
	gpio_request(GPIO_LCD_PWR, "GPIO_LCD_PWR");
	pr_err("[Kernel/LCM] GPIO_LCD_PWR = 0x%x\n", GPIO_LCD_PWR);

	GPIO_LCD_PWR2 = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr2", 0);
	gpio_request(GPIO_LCD_PWR2, "GPIO_LCD_PWR2");
	pr_err("[Kernel/LCM] GPIO_LCD_PWR2 = 0x%x\n", GPIO_LCD_PWR2);

	GPIO_LCD_BL_EN = of_get_named_gpio(dev->of_node, "gpio_lcd_bl_en", 0);
	gpio_request(GPIO_LCD_BL_EN, "GPIO_LCD_BL_EN");
	pr_err("[Kernel/LCM] GPIO_LCD_BL_EN = 0x%x\n", GPIO_LCD_BL_EN);

	GPIO_LCD_RST = of_get_named_gpio(dev->of_node, "gpio_lcd_rst", 0);
	gpio_request(GPIO_LCD_RST, "GPIO_LCD_RST");
	pr_err("[Kernel/LCM] GPIO_LCD_RST = 0x%x\n", GPIO_LCD_RST);

	return 0;
}

static int lcm_gpio_probe(struct platform_device *pdev)
{
	struct device	*dev = &pdev->dev;
	pr_err("Kernel/LCM: %s start \n",__func__);

	lcm_get_gpio(dev);
	lcm_get_vgp_supply(dev);
	lcm_vgp_supply_enable();
	return 0;
}

static const struct of_device_id lcm_of_ids[] = {
	{.compatible = "mediatek,auo_wuxga_incell_dsi",},
	{}
};

static struct platform_driver lcm_driver = {
    .probe = lcm_gpio_probe,
	.driver = {
		   .name = "auo_wuxga_incell_dsi",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = lcm_of_ids,
#endif
		   },
};

static int __init lcm_init(void)
{
//	i2c_add_driver(&lcd_i2c_driver);
	pr_notice("LCM: Register lcm driver\n");
	if (platform_driver_register(&lcm_driver)) {
		pr_err("LCM: failed to register disp driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_exit(void)
{
//	i2c_del_driver(&lcd_i2c_driver);
	platform_driver_unregister(&lcm_driver);
	pr_notice("LCM: Unregister lcm driver done\n");
}
late_initcall(lcm_init);
module_exit(lcm_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("Display subsystem Driver");
MODULE_LICENSE("GPL");

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define FRAME_WIDTH  (1200)
#define FRAME_HEIGHT (1920)

#ifndef	GPIO_OUT_ZERO
#define	GPIO_OUT_ZERO	(0)
#endif

#ifndef	GPIO_OUT_ONE
#define	GPIO_OUT_ONE	(1)
#endif

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
static struct LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)

#define MDELAY(n) (lcm_util.mdelay(n))

#define REGFLAG_DELAY             0xFEFE
#define REGFLAG_END_OF_TABLE      0xFFFF   // END OF REGISTERS MARKER

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0xFF, 1, {0x10}},
	{0xB9, 1, {0x01}},
	{0xFF, 1, {0x20}},
	{0x18, 1, {0x40}},
	{0xFF, 1, {0x10}},
	{0xB9, 1, {0x02}},
	{0xFF, 1, {0xF0}},
	{0xFB, 1, {0x01}},
	{0x3A, 1, {0x08}},
	{0xFF, 1, {0x20}},
	{0xFB, 1, {0x01}},

	{0x05, 1, {0xC9}},
	{0x06, 1, {0xC0}},
	{0x07, 1, {0x8C}},
	{0x08, 1, {0x32}},
	{0x0D, 1, {0x63}},
	{0x0E, 1, {0xA5}},
	{0x0F, 1, {0x4B}},
	{0x30, 1, {0x10}},

	{0x58, 1, {0x40}},

	{0x75, 1, {0xA2}},

	{0x88, 1, {0x00}},
	{0x94, 1, {0x00}},
	{0x95, 1, {0xEB}},
	{0x96, 1, {0xEB}},
	{0xF2, 1, {0x51}},

	{0xFF, 1, {0x24}},

	{0xFB, 1, {0x01}},

	{0x00, 1, {0x00}},
	{0x01, 1, {0x05}},
	{0x02, 1, {0x22}},
	{0x03, 1, {0x23}},
	{0x04, 1, {0x0C}},
	{0x05, 1, {0x0C}},
	{0x06, 1, {0x0D}},
	{0x07, 1, {0x0D}},
	{0x08, 1, {0x0E}},
	{0x09, 1, {0x0E}},
	{0x0A, 1, {0x0F}},
	{0x0B, 1, {0x0F}},
	{0x0C, 1, {0x10}},
	{0x0D, 1, {0x10}},
	{0x0E, 1, {0x11}},
	{0x0F, 1, {0x11}},
	{0x10, 1, {0x12}},
	{0x11, 1, {0x12}},
	{0x12, 1, {0x13}},
	{0x13, 1, {0x13}},
	{0x14, 1, {0x04}},
	{0x15, 1, {0x00}},
	{0x16, 1, {0x00}},
	{0x17, 1, {0x05}},
	{0x18, 1, {0x24}},
	{0x19, 1, {0x25}},
	{0x1A, 1, {0x0C}},
	{0x1B, 1, {0x0C}},
	{0x1C, 1, {0x0D}},
	{0x1D, 1, {0x0D}},
	{0x1E, 1, {0x0E}},
	{0x1F, 1, {0x0E}},
	{0x20, 1, {0x0F}},
	{0x21, 1, {0x0F}},
	{0x22, 1, {0x10}},
	{0x23, 1, {0x10}},
	{0x24, 1, {0x11}},
	{0x25, 1, {0x11}},
	{0x26, 1, {0x12}},
	{0x27, 1, {0x12}},
	{0x28, 1, {0x13}},
	{0x29, 1, {0x13}},
	{0x2A, 1, {0x04}},
	{0x2B, 1, {0x00}},

	{0x2D, 1, {0x00}},
	{0x2F, 1, {0x0D}},
	{0x30, 1, {0x40}},
	{0x33, 1, {0x40}},
	{0x34, 1, {0x0D}},
	{0x37, 1, {0x77}},
	{0x39, 1, {0x00}},
	{0x3A, 1, {0x01}},
	{0x3B, 1, {0x63}},
	{0x3D, 1, {0x92}},
	{0xAB, 1, {0x77}},

	{0x4D, 1, {0x15}},
	{0x4E, 1, {0x26}},
	{0x4F, 1, {0x37}},
	{0x50, 1, {0x48}},
	{0x51, 1, {0x84}},
	{0x52, 1, {0x73}},
	{0x53, 1, {0x62}},
	{0x54, 1, {0x51}},
	{0x55, 2, {0x8E,0x0E}},
	{0x56, 1, {0x08}},
	{0x58, 1, {0x21}},
	{0x59, 1, {0x70}},
	{0x5A, 1, {0x01}},
	{0x5B, 1, {0x63}},
	{0x5C, 1, {0x00}},
	{0x5D, 1, {0x00}},
	{0x5E, 2, {0x00,0x10}},

	{0x60, 1, {0x96}},
	{0x61, 1, {0x80}},
	{0x63, 1, {0x70}},

	{0x91, 1, {0x44}},

	{0x92, 1, {0x78}},
	{0x93, 1, {0x1A}},
	{0x94, 1, {0x5B}},

	{0x97, 1, {0xC2}},

	{0xB6, 12, {0x05,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x05,0x05,0x00,0x00}},

	{0xC2, 1, {0xC6}},

	{0xC3, 1, {0x0A}},
	{0xC4, 1, {0x23}},
	{0xC5, 1, {0x03}},
	{0xC6, 1, {0x59}},
	{0xC7, 1, {0x0F}},
	{0xC8, 1, {0x17}},
	{0xC9, 1, {0x0F}},
	{0xCA, 1, {0x17}},


	{0xDB, 1, {0x01}},
	{0xDC, 1, {0x79}},
	{0xDF, 1, {0x01}},
	{0xE0, 1, {0x79}},
	{0xE1, 1, {0x01}},
	{0xE2, 1, {0x79}},
	{0xE3, 1, {0x01}},
	{0xE4, 1, {0x79}},
	{0xE5, 1, {0x01}},
	{0xE6, 1, {0x79}},
	{0xEF, 1, {0x01}},
	{0xF0, 1, {0x79}},


	{0xFF, 1, {0x25}},

	{0xFB, 1, {0x01}},

	{0x05, 1, {0x00}},

	{0x13, 1, {0x03}},
	{0x14, 1, {0x11}},

	{0x19, 1, {0x07}},
	{0x1B, 1, {0x11}},

	{0x1E, 1, {0x00}},
	{0x1F, 1, {0x01}},
	{0x20, 1, {0x63}},
	{0x25, 1, {0x00}},
	{0x26, 1, {0x01}},
	{0x27, 1, {0x63}},
	{0x3F, 1, {0x80}},
	{0x40, 1, {0x08}},
	{0x43, 1, {0x00}},
	{0x44, 1, {0x01}},
	{0x45, 1, {0x63}},
	{0x48, 1, {0x01}},
	{0x49, 1, {0x63}},

	{0x5B, 1, {0x80}},
	{0x5C, 1, {0x00}},
	{0x5D, 1, {0x01}},
	{0x5E, 1, {0x63}},
	{0x61, 1, {0x01}},
	{0x62, 1, {0x63}},
	{0x67, 1, {0x00}},
	{0x68, 1, {0x0F}},

	{0xC2, 1, {0x40}},


	{0xFF, 1, {0x26}},

	{0xFB, 1, {0x01}},

	{0x00, 1, {0xA1}},

	{0x04, 1, {0x28}},

	{0x0A, 1, {0xF3}},

	{0x0C, 1, {0x13}},
	{0x0D, 1, {0x0A}},
	{0x0F, 1, {0x0A}},
	{0x11, 1, {0x00}},
	{0x12, 1, {0x50}},
	{0x13, 1, {0x4D}},
	{0x14, 1, {0x61}},
	{0x16, 1, {0x10}},
	{0x19, 1, {0x0A}},
	{0x1A, 1, {0x80}},

	{0x1B, 1, {0x09}},
	{0x1C, 1, {0xC0}},

	{0x1D, 1, {0x00}},
	{0x1E, 1, {0x78}},
	{0x1F, 1, {0x78}},
	{0x20, 1, {0x00}},
	{0x21, 1, {0x00}},
	{0x24, 1, {0x00}},
	{0x25, 1, {0x78}},

	{0x2A, 1, {0x0A}},
	{0x2B, 1, {0x80}},

	{0x2F, 1, {0x08}},
	{0x30, 1, {0x78}},
	{0x33, 1, {0x77}},
	{0x34, 1, {0x77}},
	{0x35, 1, {0x77}},
	{0x36, 1, {0x67}},
	{0x37, 1, {0x11}},
	{0x38, 1, {0x01}},

	{0x39, 1, {0x10}},
	{0x3A, 1, {0x78}},

	{0x02, 1, {0x31}},
	{0x31, 1, {0x08}},
	{0x32, 1, {0x90}},




	{0xFF, 1, {0x27}},

	{0xFB, 1, {0x01}},

	{0x56, 1, {0x06}},

	{0x58, 1, {0x80}},
	{0x59, 1, {0x46}},
	{0x5A, 1, {0x00}},
	{0x5B, 1, {0x13}},
	{0x5C, 1, {0x00}},
	{0x5D, 1, {0x01}},
	{0x5E, 1, {0x20}},
	{0x5F, 1, {0x10}},
	{0x60, 1, {0x00}},
	{0x61, 1, {0x11}},
	{0x62, 1, {0x00}},
	{0x63, 1, {0x01}},
	{0x64, 1, {0x21}},
	{0x65, 1, {0x0F}},
	{0x66, 1, {0x00}},
	{0x67, 1, {0x01}},
	{0x68, 1, {0x22}},

	{0xC0, 1, {0x00}},

	{0xFF, 1, {0x2A}},

	{0xFB, 1, {0x01}},

	{0x22, 1, {0x2F}},
	{0x23, 1, {0x08}},

	{0x24, 1, {0x00}},
	{0x25, 1, {0x76}},
	{0x27, 1, {0x00}},
	{0x28, 1, {0x1A}},
	{0x29, 1, {0x00}},
	{0x2A, 1, {0x1A}},
	{0x2B, 1, {0x00}},
	{0x2D, 1, {0x1A}},


	{0x64, 1, {0x41}},
	{0x6A, 1, {0x41}},
	{0x79, 1, {0x41}},
	{0x7C, 1, {0x41}},
	{0x7F, 1, {0x41}},
	{0x82, 1, {0x41}},
	{0x85, 1, {0x41}},

	{0x97, 1, {0x3C}},
	{0x98, 1, {0x02}},
	{0x99, 1, {0x95}},
	{0x9A, 1, {0x06}},
	{0x9B, 1, {0x00}},
	{0x9C, 1, {0x0B}},
	{0x9D, 1, {0x0A}},
	{0x9E, 1, {0x90}},

	{0xA2, 1, {0x33}},
	{0xA3, 1, {0xF0}},
	{0xA4, 1, {0x30}},

	{0xE8, 1, {0x2A}},


	{0xFF, 1, {0x23}},

	{0xFB, 1, {0x01}},

	{0x00, 1, {0x80}},
	{0x07, 1, {0x00}},
	{0x08, 1, {0x01}},
	{0x09, 1, {0x2C}},
	{0x11, 1, {0x01}},
	{0x12, 1, {0x68}},
	{0x15, 1, {0xE9}},
	{0x16, 1, {0x0C}},

	{0xFF, 1, {0x2A}},
	{0xFB, 1, {0x01}},
	{0xF1, 1, {0x03}},

	{0xFF, 1, {0xD0}},
	{0xFB, 1, {0x01}},
	{0x00, 1, {0x33}},

//CMD2_Page0
	{0xFF, 1, {0x20}},
	{0xFB, 1, {0x01}},
//R(+)
	{0xB0, 16, {0x00 ,0x08 ,0x00 ,0x1F ,0x00 ,0x41 ,0x00 ,0x5D ,0x00 ,0x75 ,0x00 ,0x8B ,0x00 ,0x9E ,0x00 ,0xB0}},
	{0xB1, 16, {0x00 ,0xC0 ,0x00 ,0xF7 ,0x01 ,0x1F ,0x01 ,0x5E ,0x01 ,0x8B ,0x01 ,0xD3 ,0x02 ,0x07 ,0x02 ,0x08}},
	{0xB2, 16, {0x02 ,0x3E ,0x02 ,0x7D ,0x02 ,0xAF ,0x02 ,0xEE ,0x03 ,0x19 ,0x03 ,0x4E ,0x03 ,0x61 ,0x03 ,0x73}},
	{0xB3, 12, {0x03 ,0x88 ,0x03 ,0x9F ,0x03 ,0xB8 ,0x03 ,0xE8 ,0x03 ,0xFD ,0x03 ,0xFF}},
//G(+)
	{0xB4, 16, {0x00 ,0x08 ,0x00 ,0x1F ,0x00 ,0x41 ,0x00 ,0x5D ,0x00 ,0x75 ,0x00 ,0x8B ,0x00 ,0x9E ,0x00 ,0xB0}},
	{0xB5, 16, {0x00 ,0xC0 ,0x00 ,0xF7 ,0x01 ,0x1F ,0x01 ,0x5E ,0x01 ,0x8B ,0x01 ,0xD3 ,0x02 ,0x07 ,0x02 ,0x08}},
	{0xB6, 16, {0x02 ,0x3E ,0x02 ,0x7D ,0x02 ,0xAF ,0x02 ,0xEE ,0x03 ,0x19 ,0x03 ,0x4E ,0x03 ,0x61 ,0x03 ,0x73}},
	{0xB7, 12, {0x03 ,0x88 ,0x03 ,0x9F ,0x03 ,0xB8 ,0x03 ,0xE8 ,0x03 ,0xFD ,0x03 ,0xFF}},
//B(+)
	{0xB8, 16, {0x00 ,0x08 ,0x00 ,0x1F ,0x00 ,0x41 ,0x00 ,0x5D ,0x00 ,0x75 ,0x00 ,0x8B ,0x00 ,0x9E ,0x00 ,0xB0}},
	{0xB9, 16, {0x00 ,0xC0 ,0x00 ,0xF7 ,0x01 ,0x1F ,0x01 ,0x5E ,0x01 ,0x8B ,0x01 ,0xD3 ,0x02 ,0x07 ,0x02 ,0x08}},
	{0xBA, 16, {0x02 ,0x3E ,0x02 ,0x7D ,0x02 ,0xAF ,0x02 ,0xEE ,0x03 ,0x19 ,0x03 ,0x4E ,0x03 ,0x61 ,0x03 ,0x73}},
	{0xBB, 12, {0x03 ,0x88 ,0x03 ,0x9F ,0x03 ,0xB8 ,0x03 ,0xE8 ,0x03 ,0xFD ,0x03 ,0xFF}},

//CMD2_Page1
	{0xFF, 1 , {0x21}},
	{0xFB, 1 , {0x01}},
//R(-)
	{0xB0, 16, {0x00 ,0x00 ,0x00 ,0x17 ,0x00 ,0x39 ,0x00 ,0x55 ,0x00 ,0x6D ,0x00 ,0x83 ,0x00 ,0x96 ,0x00 ,0xA8}},
	{0xB1, 16, {0x00 ,0xB8 ,0x00 ,0xEF ,0x01 ,0x17 ,0x01 ,0x56 ,0x01 ,0x83 ,0x01 ,0xCB ,0x01 ,0xFF ,0x02 ,0x00}},
	{0xB2, 16, {0x02 ,0x36 ,0x02 ,0x75 ,0x02 ,0xA7 ,0x02 ,0xE6 ,0x03 ,0x11 ,0x03 ,0x46 ,0x03 ,0x59 ,0x03 ,0x6B}},
	{0xB3, 12, {0x03 ,0x80 ,0x03 ,0x97 ,0x03 ,0xB0 ,0x03 ,0xE0 ,0x03 ,0xF5 ,0x03 ,0xF7}},
//G(-)
	{0xB4, 16, {0x00 ,0x00 ,0x00 ,0x17 ,0x00 ,0x39 ,0x00 ,0x55 ,0x00 ,0x6D ,0x00 ,0x83 ,0x00 ,0x96 ,0x00 ,0xA8}},
	{0xB5, 16, {0x00 ,0xB8 ,0x00 ,0xEF ,0x01 ,0x17 ,0x01 ,0x56 ,0x01 ,0x83 ,0x01 ,0xCB ,0x01 ,0xFF ,0x02 ,0x00}},
	{0xB6, 16, {0x02 ,0x36 ,0x02 ,0x75 ,0x02 ,0xA7 ,0x02 ,0xE6 ,0x03 ,0x11 ,0x03 ,0x46 ,0x03 ,0x59 ,0x03 ,0x6B}},
	{0xB7, 12, {0x03 ,0x80 ,0x03 ,0x97 ,0x03 ,0xB0 ,0x03 ,0xE0 ,0x03 ,0xF5 ,0x03 ,0xF7}},
//B(-)
	{0xB8, 16, {0x00 ,0x00 ,0x00 ,0x17 ,0x00 ,0x39 ,0x00 ,0x55 ,0x00 ,0x6D ,0x00 ,0x83 ,0x00 ,0x96 ,0x00 ,0xA8}},
	{0xB9, 16, {0x00 ,0xB8 ,0x00 ,0xEF ,0x01 ,0x17 ,0x01 ,0x56 ,0x01 ,0x83 ,0x01 ,0xCB ,0x01 ,0xFF ,0x02 ,0x00}},
	{0xBA, 16, {0x02 ,0x36 ,0x02 ,0x75 ,0x02 ,0xA7 ,0x02 ,0xE6 ,0x03 ,0x11 ,0x03 ,0x46 ,0x03 ,0x59 ,0x03 ,0x6B}},
	{0xBB, 12, {0x03 ,0x80 ,0x03 ,0x97 ,0x03 ,0xB0 ,0x03 ,0xE0 ,0x03 ,0xF5 ,0x03 ,0xF7}},

//#####TEST#############
	{0xFF, 1, {0x27}},
	{0xD1, 1, {0x44}},
	{0xD2, 1, {0x57}},

//######################

	{0xFF, 1, {0x10}},

	{0xFB, 1, {0x01}},

	{0x35, 1, {0x00}},
	{0x51, 2, {0x0F,0xFF}},
	{0x53, 1, {0x2C}},
	{0x55, 1, {0x00}},
	{0xBB, 1, {0x13}},
	{0x3B, 5, {0x03,0x5B,0x1A,0x04,0x04}},
	{REGFLAG_DELAY, 5, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {0x00}}
};

static struct LCM_setting_table lcm_initialization_setting2[] = {
	{0xFF, 1, {0x10}},
	{0xB9, 1, {0x01}},
	{0xFF, 1, {0x20}},
	{0x18, 1, {0x40}},
	{0xFF, 1, {0x10}},
	{0xB9, 1, {0x02}},
	{0xFF, 1, {0xF0}},
	{0xFB, 1, {0x01}},
	{0x3A, 1, {0x08}},
	{0xFF, 1, {0x20}},
	{0xFB, 1, {0x01}},

	{0x05, 1, {0xC9}},
	{0x06, 1, {0xC0}},
	{0x07, 1, {0x8C}},
	{0x08, 1, {0x32}},
	{0x0D, 1, {0x63}},
	{0x0E, 1, {0xA5}},
	{0x0F, 1, {0x4B}},
	{0x30, 1, {0x10}},

	{0x58, 1, {0x40}},

	{0x75, 1, {0xA2}},

	{0x88, 1, {0x00}},
	{0x94, 1, {0x00}},
	{0x95, 1, {0xEB}},
	{0x96, 1, {0xEB}},
	{0xF2, 1, {0x51}},

	{0xFF, 1, {0x24}},

	{0xFB, 1, {0x01}},

	{0x00, 1, {0x00}},
	{0x01, 1, {0x05}},
	{0x02, 1, {0x22}},
	{0x03, 1, {0x23}},
	{0x04, 1, {0x0C}},
	{0x05, 1, {0x0C}},
	{0x06, 1, {0x0D}},
	{0x07, 1, {0x0D}},
	{0x08, 1, {0x0E}},
	{0x09, 1, {0x0E}},
	{0x0A, 1, {0x0F}},
	{0x0B, 1, {0x0F}},
	{0x0C, 1, {0x10}},
	{0x0D, 1, {0x10}},
	{0x0E, 1, {0x11}},
	{0x0F, 1, {0x11}},
	{0x10, 1, {0x12}},
	{0x11, 1, {0x12}},
	{0x12, 1, {0x13}},
	{0x13, 1, {0x13}},
	{0x14, 1, {0x04}},
	{0x15, 1, {0x00}},
	{0x16, 1, {0x00}},
	{0x17, 1, {0x05}},
	{0x18, 1, {0x24}},
	{0x19, 1, {0x25}},
	{0x1A, 1, {0x0C}},
	{0x1B, 1, {0x0C}},
	{0x1C, 1, {0x0D}},
	{0x1D, 1, {0x0D}},
	{0x1E, 1, {0x0E}},
	{0x1F, 1, {0x0E}},
	{0x20, 1, {0x0F}},
	{0x21, 1, {0x0F}},
	{0x22, 1, {0x10}},
	{0x23, 1, {0x10}},
	{0x24, 1, {0x11}},
	{0x25, 1, {0x11}},
	{0x26, 1, {0x12}},
	{0x27, 1, {0x12}},
	{0x28, 1, {0x13}},
	{0x29, 1, {0x13}},
	{0x2A, 1, {0x04}},
	{0x2B, 1, {0x00}},

	{0x2D, 1, {0x00}},
	{0x2F, 1, {0x0E}},
	{0x30, 1, {0x40}},
	{0x33, 1, {0x40}},
	{0x34, 1, {0x0E}},
	{0x37, 1, {0x88}},
	{0x39, 1, {0x00}},
	{0x3A, 1, {0x01}},
	{0x3B, 1, {0x63}},
	{0x3D, 1, {0x92}},
	{0xAB, 1, {0x77}},

	{0x4D, 1, {0x15}},
	{0x4E, 1, {0x26}},
	{0x4F, 1, {0x37}},
	{0x50, 1, {0x48}},
	{0x51, 1, {0x84}},
	{0x52, 1, {0x73}},
	{0x53, 1, {0x62}},
	{0x54, 1, {0x51}},
	{0x55, 2, {0x8F,0x0F}},
	{0x56, 1, {0x08}},
	{0x58, 1, {0x21}},
	{0x59, 1, {0x80}},
	{0x5A, 1, {0x01}},
	{0x5B, 1, {0x63}},
	{0x5C, 1, {0x00}},
	{0x5D, 1, {0x00}},
	{0x5E, 2, {0x00,0x10}},

	{0x60, 1, {0x96}},
	{0x61, 1, {0x80}},
	{0x63, 1, {0x70}},

	{0x91, 1, {0x44}},
	{0x92, 1, {0x78}},
	{0x93, 1, {0x1A}},
	{0x94, 1, {0x5B}},

	{0x97, 1, {0xC2}},

	{0xB6, 12, {0x05,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x05,0x05,0x00,0x00}},

	{0xC2, 1, {0xC6}},

	{0xC3, 1, {0x0A}},
	{0xC4, 1, {0x23}},
	{0xC5, 1, {0x03}},
	{0xC6, 1, {0x59}},
	{0xC7, 1, {0x0F}},
	{0xC8, 1, {0x17}},
	{0xC9, 1, {0x0F}},
	{0xCA, 1, {0x17}},


	{0xDB, 1, {0x01}},
	{0xDC, 1, {0x79}},
	{0xDF, 1, {0x01}},
	{0xE0, 1, {0x79}},
	{0xE1, 1, {0x01}},
	{0xE2, 1, {0x79}},
	{0xE3, 1, {0x01}},
	{0xE4, 1, {0x79}},
	{0xE5, 1, {0x01}},
	{0xE6, 1, {0x79}},
	{0xEF, 1, {0x01}},
	{0xF0, 1, {0x79}},


	{0xFF, 1, {0x25}},

	{0xFB, 1, {0x01}},

	{0x05, 1, {0x00}},

	{0x13, 1, {0x03}},
	{0x14, 1, {0x11}},

	{0x19, 1, {0x07}},
	{0x1B, 1, {0x11}},

	{0x1E, 1, {0x00}},
	{0x1F, 1, {0x01}},
	{0x20, 1, {0x63}},
	{0x25, 1, {0x00}},
	{0x26, 1, {0x01}},
	{0x27, 1, {0x63}},
	{0x3F, 1, {0x80}},
	{0x40, 1, {0x08}},
	{0x43, 1, {0x00}},
	{0x44, 1, {0x01}},
	{0x45, 1, {0x63}},
	{0x48, 1, {0x01}},
	{0x49, 1, {0x63}},

	{0x5B, 1, {0x80}},
	{0x5C, 1, {0x00}},
	{0x5D, 1, {0x01}},
	{0x5E, 1, {0x63}},
	{0x61, 1, {0x01}},
	{0x62, 1, {0x63}},
	{0x67, 1, {0x00}},
	{0x68, 1, {0x0F}},

	{0xC2, 1, {0x40}},


	{0xFF, 1, {0x26}},

	{0xFB, 1, {0x01}},

	{0x00, 1, {0xA1}},

	{0x04, 1, {0x28}},

	{0x0A, 1, {0xF3}},

	{0x0C, 1, {0x13}},
	{0x0D, 1, {0x0A}},
	{0x0F, 1, {0x0A}},
	{0x11, 1, {0x00}},
	{0x12, 1, {0x50}},
	{0x13, 1, {0x4D}},
	{0x14, 1, {0x61}},
	{0x16, 1, {0x10}},
	{0x19, 1, {0x0A}},
	{0x1A, 1, {0x80}},
	{0x1B, 1, {0x09}},
	{0x1C, 1, {0xC0}},
	{0x1D, 1, {0x00}},
	{0x1E, 1, {0x78}},
	{0x1F, 1, {0x78}},
	{0x20, 1, {0x00}},
	{0x21, 1, {0x00}},
	{0x24, 1, {0x00}},
	{0x25, 1, {0x78}},
	{0x2A, 1, {0x0A}},
	{0x2B, 1, {0x80}},
	{0x2F, 1, {0x08}},
	{0x30, 1, {0x78}},
	{0x33, 1, {0x77}},
	{0x34, 1, {0x77}},
	{0x35, 1, {0x77}},
	{0x36, 1, {0x67}},
	{0x37, 1, {0x11}},
	{0x38, 1, {0x01}},

	{0x39, 1, {0x10}},
	{0x3A, 1, {0x78}},

	{0x02, 1, {0x31}},
	{0x31, 1, {0x08}},
	{0x32, 1, {0x90}},

	{0xFF, 1, {0x27}},

	{0xFB, 1, {0x01}},

	{0x56, 1, {0x06}},

	{0x58, 1, {0x80}},
	{0x59, 1, {0x46}},
	{0x5A, 1, {0x00}},
	{0x5B, 1, {0x13}},
	{0x5C, 1, {0x00}},
	{0x5D, 1, {0x01}},
	{0x5E, 1, {0x20}},
	{0x5F, 1, {0x10}},
	{0x60, 1, {0x00}},
	{0x61, 1, {0x11}},
	{0x62, 1, {0x00}},
	{0x63, 1, {0x01}},
	{0x64, 1, {0x21}},
	{0x65, 1, {0x0F}},
	{0x66, 1, {0x00}},
	{0x67, 1, {0x01}},
	{0x68, 1, {0x22}},

	{0xC0, 1, {0x00}},

	{0xFF, 1, {0x2A}},

	{0xFB, 1, {0x01}},

	{0x22, 1, {0x2F}},
	{0x23, 1, {0x08}},

	{0x24, 1, {0x00}},
	{0x25, 1, {0x76}},
	{0x27, 1, {0x00}},
	{0x28, 1, {0x1A}},
	{0x29, 1, {0x00}},
	{0x2A, 1, {0x1A}},
	{0x2B, 1, {0x00}},
	{0x2D, 1, {0x1A}},


	{0x64, 1, {0x41}},
	{0x6A, 1, {0x41}},
	{0x79, 1, {0x41}},
	{0x7C, 1, {0x41}},
	{0x7F, 1, {0x41}},
	{0x82, 1, {0x41}},
	{0x85, 1, {0x41}},

	{0x97, 1, {0x3C}},
	{0x98, 1, {0x02}},
	{0x99, 1, {0x95}},
	{0x9A, 1, {0x06}},
	{0x9B, 1, {0x00}},
	{0x9C, 1, {0x0B}},
	{0x9D, 1, {0x0A}},
	{0x9E, 1, {0x90}},

	{0xA2, 1, {0x33}},
	{0xA3, 1, {0xF0}},
	{0xA4, 1, {0x30}},

	{0xE8, 1, {0x2A}},


	{0xFF, 1, {0x23}},

	{0xFB, 1, {0x01}},

	{0x00, 1, {0x80}},
	{0x07, 1, {0x00}},
	{0x08, 1, {0x01}},
	{0x09, 1, {0x2C}},
	{0x11, 1, {0x01}},
	{0x12, 1, {0x68}},
	{0x15, 1, {0xE9}},
	{0x16, 1, {0x0C}},

	{0xFF, 1, {0x2A}},
	{0xFB, 1, {0x01}},
	{0xF1, 1, {0x03}},

	{0xFF, 1, {0xD0}},
	{0xFB, 1, {0x01}},
	{0x00, 1, {0x33}},

//CMD2_Page0
	{0xFF, 1, {0x20}},
	{0xFB, 1, {0x01}},
//R(+)
	{0xB0, 16, {0x00 ,0x08 ,0x00 ,0x1B ,0x00 ,0x3A ,0x00 ,0x53 ,0x00 ,0x69 ,0x00 ,0x7D ,0x00 ,0x8F ,0x00 ,0xA0}},
	{0xB1, 16, {0x00 ,0xAF ,0x00 ,0xE4 ,0x01 ,0x0A ,0x01 ,0x47 ,0x01 ,0x73 ,0x01 ,0xBB ,0x01 ,0xF1 ,0x01 ,0xF2}},
	{0xB2, 16, {0x02 ,0x29 ,0x02 ,0x6D ,0x02 ,0xA0 ,0x02 ,0xE0 ,0x03 ,0x0E ,0x03 ,0x42 ,0x03 ,0x55 ,0x03 ,0x67}},
	{0xB3, 12, {0x03 ,0x7C ,0x03 ,0x93 ,0x03 ,0xAC ,0x03 ,0xDA ,0x03 ,0xFC ,0x03 ,0xFF}},
//G(+)
	{0xB4, 16, {0x00 ,0x08 ,0x00 ,0x1B ,0x00 ,0x3A ,0x00 ,0x53 ,0x00 ,0x69 ,0x00 ,0x7D ,0x00 ,0x8F ,0x00 ,0xA0}},
	{0xB5, 16, {0x00 ,0xAF ,0x00 ,0xE4 ,0x01 ,0x0A ,0x01 ,0x47 ,0x01 ,0x73 ,0x01 ,0xBB ,0x01 ,0xF1 ,0x01 ,0xF2}},
	{0xB6, 16, {0x02 ,0x29 ,0x02 ,0x6D ,0x02 ,0xA0 ,0x02 ,0xE0 ,0x03 ,0x0E ,0x03 ,0x42 ,0x03 ,0x55 ,0x03 ,0x67}},
	{0xB7, 12, {0x03 ,0x7C ,0x03 ,0x93 ,0x03 ,0xAC ,0x03 ,0xDA ,0x03 ,0xFC ,0x03 ,0xFF}},
//B(+)
	{0xB8, 16, {0x00 ,0x08 ,0x00 ,0x1B ,0x00 ,0x3A ,0x00 ,0x53 ,0x00 ,0x69 ,0x00 ,0x7D ,0x00 ,0x8F ,0x00 ,0xA0}},
	{0xB9, 16, {0x00 ,0xAF ,0x00 ,0xE4 ,0x01 ,0x0A ,0x01 ,0x47 ,0x01 ,0x73 ,0x01 ,0xBB ,0x01 ,0xF1 ,0x01 ,0xF2}},
	{0xBA, 16, {0x02 ,0x29 ,0x02 ,0x6D ,0x02 ,0xA0 ,0x02 ,0xE0 ,0x03 ,0x0E ,0x03 ,0x42 ,0x03 ,0x55 ,0x03 ,0x67}},
	{0xBB, 12, {0x03 ,0x7C ,0x03 ,0x93 ,0x03 ,0xAC ,0x03 ,0xDA ,0x03 ,0xFC ,0x03 ,0xFF}},

	{0xC6, 1, {0x11}},
	{0xC7, 1, {0x11}},
	{0xC8, 1, {0x11}},
	{0xC9, 1, {0x11}},
	{0xCA, 1, {0x11}},

	{0xCB, 1, {0x11}},
	{0xCC, 1, {0x11}},
	{0xCD, 1, {0x11}},
	{0xCE, 1, {0x11}},
	{0xCF, 1, {0x11}},

	{0xD0, 1, {0x11}},
	{0xD1, 1, {0x11}},
	{0xD2, 1, {0x11}},
	{0xD3, 1, {0x11}},
	{0xD4, 1, {0x11}},

	{0xD5, 1, {0x11}},
	{0xD6, 1, {0x11}},
	{0xD7, 1, {0x11}},
	{0xD8, 1, {0x11}},
	{0xD9, 1, {0x11}},

	{0xDA, 1, {0x11}},
	{0xDB, 1, {0x11}},
	{0xDC, 1, {0x11}},
	{0xDD, 1, {0x11}},
	{0xDE, 1, {0x11}},

	{0xDF, 1, {0x11}},
	{0xE0, 1, {0x11}},
	{0xE1, 1, {0x11}},
	{0xE2, 1, {0x11}},
	{0xE3, 1, {0x11}},

//CMD2_Page1
	{0xFF, 1 , {0x21}},
	{0xFB, 1 , {0x01}},
//R(-)
	{0xB0, 16, {0x00 ,0x00 ,0x00 ,0x13 ,0x00 ,0x32 ,0x00 ,0x4B ,0x00 ,0x61 ,0x00 ,0x75 ,0x00 ,0x87 ,0x00 ,0x98}},
	{0xB1, 16, {0x00 ,0xA7 ,0x00 ,0xDC ,0x01 ,0x02 ,0x01 ,0x3F ,0x01 ,0x6B ,0x01 ,0xB3 ,0x01 ,0xE9 ,0x01 ,0xEA}},
	{0xB2, 16, {0x02 ,0x21 ,0x02 ,0x65 ,0x02 ,0x98 ,0x02 ,0xD8 ,0x03 ,0x06 ,0x03 ,0x3A ,0x03 ,0x4D ,0x03 ,0x5F}},
	{0xB3, 12, {0x03 ,0x74 ,0x03 ,0x8B ,0x03 ,0xA4 ,0x03 ,0xD2 ,0x03 ,0xF4 ,0x03 ,0xF7}},
//G(-)
	{0xB4, 16, {0x00 ,0x00 ,0x00 ,0x13 ,0x00 ,0x32 ,0x00 ,0x4B ,0x00 ,0x61 ,0x00 ,0x75 ,0x00 ,0x87 ,0x00 ,0x98}},
	{0xB5, 16, {0x00 ,0xA7 ,0x00 ,0xDC ,0x01 ,0x02 ,0x01 ,0x3F ,0x01 ,0x6B ,0x01 ,0xB3 ,0x01 ,0xE9 ,0x01 ,0xEA}},
	{0xB6, 16, {0x02 ,0x21 ,0x02 ,0x65 ,0x02 ,0x98 ,0x02 ,0xD8 ,0x03 ,0x06 ,0x03 ,0x3A ,0x03 ,0x4D ,0x03 ,0x5F}},
	{0xB7, 12, {0x03 ,0x74 ,0x03 ,0x8B ,0x03 ,0xA4 ,0x03 ,0xD2 ,0x03 ,0xF4 ,0x03 ,0xF7}},
//B(-)
	{0xB8, 16, {0x00 ,0x00 ,0x00 ,0x13 ,0x00 ,0x32 ,0x00 ,0x4B ,0x00 ,0x61 ,0x00 ,0x75 ,0x00 ,0x87 ,0x00 ,0x98}},
	{0xB9, 16, {0x00 ,0xA7 ,0x00 ,0xDC ,0x01 ,0x02 ,0x01 ,0x3F ,0x01 ,0x6B ,0x01 ,0xB3 ,0x01 ,0xE9 ,0x01 ,0xEA}},
	{0xBA, 16, {0x02 ,0x21 ,0x02 ,0x65 ,0x02 ,0x98 ,0x02 ,0xD8 ,0x03 ,0x06 ,0x03 ,0x3A ,0x03 ,0x4D ,0x03 ,0x5F}},
	{0xBB, 12, {0x03 ,0x74 ,0x03 ,0x8B ,0x03 ,0xA4 ,0x03 ,0xD2 ,0x03 ,0xF4 ,0x03 ,0xF7}},

//#####TEST#############


	{0xFF, 1, {0x2A}},
	{0xFB, 1, {0x01}},
	{0x25, 1, {0x76}},

	{0xFF, 1, {0x25}},
	{0xFB, 1, {0x01}},
	{0x40, 1, {0x07}},

//######################

	{0xFF, 1, {0x10}},

	{0xFB, 1, {0x01}},

	{0x35, 1, {0x00}},
	{0x51, 2, {0x0F,0xFF}},
	{0x53, 1, {0x2C}},
	{0x55, 1, {0x00}},
	{0xBB, 1, {0x13}},
	{0x3B, 5, {0x03,0x5B,0x1A,0x04,0x04}},
	{REGFLAG_DELAY, 5, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {0x00}}
};

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	gpio_set_value(GPIO, output);
}

static void lcm_init_power(void)
{
	pr_err("[Kernel/LCM] lcm_init_power() enter\n");
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ONE);
	MDELAY(2);
	lcm_set_gpio_output(GPIO_LCD_PWR2, GPIO_OUT_ONE);
}

static void lcm_suspend_power(void)
{
	pr_err("[Kernel/LCM] lcm_suspend_power() enter\n");

//	lcm_set_gpio_output(GPIO_LCD_PWR2, GPIO_OUT_ZERO);
	MDELAY(2);
//	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ZERO);
//	MDELAY(500);
}

static void lcm_resume_power(void)
{
	pr_err("[Kernel/LCM] lcm_resume_power() enter\n");
	lcm_vgp_supply_enable();
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ONE);
	MDELAY(2);
	lcm_set_gpio_output(GPIO_LCD_PWR2, GPIO_OUT_ONE);
	MDELAY(100);
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for(i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;
		switch (cmd) {
			case REGFLAG_DELAY :
				MDELAY(table[i].count);
				break;
			case REGFLAG_END_OF_TABLE :
				break;
			default:
				dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
				break;
		}
	}
}

static void init_lcm_registers(void)
{
	unsigned char buffer;
	int id;

	read_reg_v2(0xdb, &buffer, 1);
	id = buffer;
	if(id == 0x80)
		push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	else
		push_table(lcm_initialization_setting2, sizeof(lcm_initialization_setting2) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	pr_err("[Kernel/LCM] lcm_get_params() enter\n");

	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;
	params->lcm_if = LCM_INTERFACE_DSI0;
	params->lcm_cmd_if = LCM_INTERFACE_DSI0;

	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;

	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size = 256;
	params->dsi.intermediat_buffer_num = 0;	/* This para should be 0 at video mode */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = FRAME_WIDTH * 3;

//	params->dsi.horizontal_sync_active 	= 1;
	params->dsi.horizontal_sync_active 	= 10; //auo
//	params->dsi.horizontal_backporch  	= 56;
//	params->dsi.horizontal_backporch  	= 45; //def
//	params->dsi.horizontal_backporch  	= 43; //miyawaki
	params->dsi.horizontal_backporch  	= 30; //auo
//	params->dsi.horizontal_frontporch 	= 67; //def
//	params->dsi.horizontal_frontporch 	= 70; //miyawaki
	params->dsi.horizontal_frontporch 	= 73; //auo
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

//	params->dsi.vertical_sync_active 	= 1;
	params->dsi.vertical_sync_active 	= 4; //auo
//	params->dsi.vertical_backporch  	= 90;
	params->dsi.vertical_backporch  	= 87; //auo
//	params->dsi.vertical_backporch  	= 88; //def
	params->dsi.vertical_frontporch 	= 26;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.PLL_CLOCK 		  	= 510; //def auo
//	params->dsi.PLL_CLOCK 		  	= 511; //miyawaki
	params->dsi.ssc_disable = 1;
}

static void lcm_init_lcm(void)
{
	unsigned int data_array[16];

	pr_err("[Kernel/LCM] lcm_init() enter\n");
	init_lcm_registers();

	data_array[0] = 0x00110500; // Sleep Out
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(180);

	data_array[0] = 0x00290500; // Display On
	dsi_set_cmdq(data_array, 1, 1);
}

static void lcm_suspend(void)
{
	unsigned int data_array[16];

	data_array[0] = 0x00280500; // Display Off
	dsi_set_cmdq(data_array, 1, 1);

	data_array[0] = 0x00100500; // Sleep In
	dsi_set_cmdq(data_array, 1, 1);

	MDELAY(180);

	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);

	MDELAY(120);

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	lcm_set_gpio_output(GPIO_LCD_PWR2, GPIO_OUT_ZERO);
	MDELAY(2);
	lcm_set_gpio_output(GPIO_LCD_PWR, GPIO_OUT_ZERO);
	MDELAY(150);
	lcm_vgp_supply_disable();
	MDELAY(100);

	pr_err("[Kernel/LCM] lcm_suspend() enter\n");
}

static void lcm_resume(void)
{
	unsigned int data_array[16];

	pr_err("[Kernel/LCM] lcm_resume() enter\n");

	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(100);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(100);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(100);

	init_lcm_registers();

	data_array[0] = 0x00110500; // Sleep Out
	dsi_set_cmdq(data_array, 1, 1);
	MDELAY(180);

	data_array[0] = 0x00290500; // Display On
	dsi_set_cmdq(data_array, 1, 1);

	MDELAY(33);
}

void sts_lcd_backlight_enable(bool on)
{
	if(on)
	{
		lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
	}
	else
	{
		lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ZERO);
	}
}

struct LCM_DRIVER auo_wuxga_incell_dsi_lcm_drv = {
    .name		= "auo_wuxga_incell_dsi",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init = lcm_init_lcm,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.init_power		= lcm_init_power,
	.resume_power 	= lcm_resume_power,
	.suspend_power 	= lcm_suspend_power,
};
