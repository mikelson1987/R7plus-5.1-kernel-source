/*************************************************************
 ** Copyright (C), 2012-2016, OPPO Mobile Comm Corp., Ltd 
 ** VENDOR_EDIT
 ** File        : apds993x.c
 ** Description : 
 ** Date        : 2014-10-31 
 ** Author      : BSP.Sensor
 ** 
 ** ------------------ Revision History: ---------------------
 **      <author>        <date>          <desc>
 *************************************************************/

/*
 * apds993x.c - Linux kernel modules for ambient light + proximity sensor
 *
 * Copyright (C) 2012 Lee Kai Koon <kai-koon.lee@avagotech.com>
 * Copyright (C) 2012 Avago Technologies
 * Copyright (C) 2013 LGE Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/i2c/apds993x.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/sensors.h>
#include <linux/sensors_ftm.h>
#include <linux/wakelock.h>
#include <soc/oppo/oppo_project.h>

#define APDS993X_DRV_NAME	"apds993x"
#define DRIVER_VERSION		"1.0.0"

#define APDS993X_PS_DETECTION_THRESHOLD		800
#define APDS993X_PS_HSYTERESIS_THRESHOLD	700
#define APDS993X_PS_PULSE_NUMBER		8

#define APDS993X_ALS_THRESHOLD_HSYTERESIS	20	/* % */

#define APDS993X_GA	48	/* 0.48 without glass window */
#define APDS993X_COE_B	223	/* 2.23 without glass window */
#define APDS993X_COE_C	70	/* 0.70 without glass window */
#define APDS993X_COE_D	142	/* 1.42 without glass window */
#define APDS993X_DF	52
#define ALS_MAX_RANGE	60000

/* Change History
 *
 * 1.0.0	Fundamental Functions of APDS-993x
 *
 */
#define APDS993X_IOCTL_PS_ENABLE	1
#define APDS993X_IOCTL_PS_GET_ENABLE	2
#define APDS993X_IOCTL_PS_GET_PDATA	3	/* pdata */
#define APDS993X_IOCTL_ALS_ENABLE	4
#define APDS993X_IOCTL_ALS_GET_ENABLE	5
#define APDS993X_IOCTL_ALS_GET_CH0DATA	6	/* ch0data */
#define APDS993X_IOCTL_ALS_GET_CH1DATA	7	/* ch1data */
#define APDS993X_IOCTL_ALS_DELAY	8

/*
 * Defines
 */
#define	APDS9930_ID	0x30
#define	APDS9931_ID	0x39
#define	APDS9900_ID	0x29
#define	APDS9901_ID	0x20

#define APDS993X_ENABLE_REG	0x00
#define APDS993X_ATIME_REG	0x01
#define APDS993X_PTIME_REG	0x02
#define APDS993X_WTIME_REG	0x03
#define APDS993X_AILTL_REG	0x04
#define APDS993X_AILTH_REG	0x05
#define APDS993X_AIHTL_REG	0x06
#define APDS993X_AIHTH_REG	0x07
#define APDS993X_PILTL_REG	0x08
#define APDS993X_PILTH_REG	0x09
#define APDS993X_PIHTL_REG	0x0A
#define APDS993X_PIHTH_REG	0x0B
#define APDS993X_PERS_REG	0x0C
#define APDS993X_CONFIG_REG	0x0D
#define APDS993X_PPCOUNT_REG	0x0E
#define APDS993X_CONTROL_REG	0x0F
#define APDS993X_REV_REG	0x11
#define APDS993X_ID_REG		0x12
#define APDS993X_STATUS_REG	0x13
#define APDS993X_CH0DATAL_REG	0x14
#define APDS993X_CH0DATAH_REG	0x15
#define APDS993X_CH1DATAL_REG	0x16
#define APDS993X_CH1DATAH_REG	0x17
#define APDS993X_PDATAL_REG	0x18
#define APDS993X_PDATAH_REG	0x19

#define CMD_BYTE		0x80
#define CMD_WORD		0xA0
#define CMD_SPECIAL		0xE0

#define CMD_CLR_PS_INT		0xE5
#define CMD_CLR_ALS_INT		0xE6
#define CMD_CLR_PS_ALS_INT	0xE7


/* Register Value define : ATIME */
#define APDS993X_100MS_ADC_TIME	0xDB  /* 100.64ms integration time */
#define APDS993X_50MS_ADC_TIME	0xED  /* 51.68ms integration time */
#define APDS993X_27MS_ADC_TIME	0xF6  /* 27.2ms integration time */

/* Register Value define : PRXCNFG */
#define APDS993X_ALS_REDUCE	0x04  /* ALSREDUCE - ALS Gain reduced by 4x */

/* Register Value define : PERS */
#define APDS993X_PPERS_0	0x00  /* Every proximity ADC cycle */
#define APDS993X_PPERS_1	0x10  /* 1 consecutive proximity value out of range */
#define APDS993X_PPERS_2	0x20  /* 2 consecutive proximity value out of range */
#define APDS993X_PPERS_3	0x30  /* 3 consecutive proximity value out of range */
#define APDS993X_PPERS_4	0x40  /* 4 consecutive proximity value out of range */
#define APDS993X_PPERS_5	0x50  /* 5 consecutive proximity value out of range */
#define APDS993X_PPERS_6	0x60  /* 6 consecutive proximity value out of range */
#define APDS993X_PPERS_7	0x70  /* 7 consecutive proximity value out of range */
#define APDS993X_PPERS_8	0x80  /* 8 consecutive proximity value out of range */
#define APDS993X_PPERS_9	0x90  /* 9 consecutive proximity value out of range */
#define APDS993X_PPERS_10	0xA0  /* 10 consecutive proximity value out of range */
#define APDS993X_PPERS_11	0xB0  /* 11 consecutive proximity value out of range */
#define APDS993X_PPERS_12	0xC0  /* 12 consecutive proximity value out of range */
#define APDS993X_PPERS_13	0xD0  /* 13 consecutive proximity value out of range */
#define APDS993X_PPERS_14	0xE0  /* 14 consecutive proximity value out of range */
#define APDS993X_PPERS_15	0xF0  /* 15 consecutive proximity value out of range */

#define APDS993X_APERS_0	0x00  /* Every ADC cycle */
#define APDS993X_APERS_1	0x01  /* 1 consecutive proximity value out of range */
#define APDS993X_APERS_2	0x02  /* 2 consecutive proximity value out of range */
#define APDS993X_APERS_3	0x03  /* 3 consecutive proximity value out of range */
#define APDS993X_APERS_5	0x04  /* 5 consecutive proximity value out of range */
#define APDS993X_APERS_10	0x05  /* 10 consecutive proximity value out of range */
#define APDS993X_APERS_15	0x06  /* 15 consecutive proximity value out of range */
#define APDS993X_APERS_20	0x07  /* 20 consecutive proximity value out of range */
#define APDS993X_APERS_25	0x08  /* 25 consecutive proximity value out of range */
#define APDS993X_APERS_30	0x09  /* 30 consecutive proximity value out of range */
#define APDS993X_APERS_35	0x0A  /* 35 consecutive proximity value out of range */
#define APDS993X_APERS_40	0x0B  /* 40 consecutive proximity value out of range */
#define APDS993X_APERS_45	0x0C  /* 45 consecutive proximity value out of range */
#define APDS993X_APERS_50	0x0D  /* 50 consecutive proximity value out of range */
#define APDS993X_APERS_55	0x0E  /* 55 consecutive proximity value out of range */
#define APDS993X_APERS_60	0x0F  /* 60 consecutive proximity value out of range */

/* Register Value define : CONTROL */
#define APDS993X_AGAIN_1X	0x00  /* 1X ALS GAIN */
#define APDS993X_AGAIN_8X	0x01  /* 8X ALS GAIN */
#define APDS993X_AGAIN_16X	0x02  /* 16X ALS GAIN */
#define APDS993X_AGAIN_120X	0x03  /* 120X ALS GAIN */

#define APDS993X_PRX_IR_DIOD	0x20  /* Proximity uses CH1 diode */

#define APDS993X_PGAIN_1X	0x00  /* PS GAIN 1X */
#define APDS993X_PGAIN_2X	0x04  /* PS GAIN 2X */
#define APDS993X_PGAIN_4X	0x08  /* PS GAIN 4X */
#define APDS993X_PGAIN_8X	0x0C  /* PS GAIN 8X */

#define APDS993X_PDRVIE_100MA	0x00  /* PS 100mA LED drive */
#define APDS993X_PDRVIE_50MA	0x40  /* PS 50mA LED drive */
#define APDS993X_PDRVIE_25MA	0x80  /* PS 25mA LED drive */
#define APDS993X_PDRVIE_12_5MA	0xC0  /* PS 12.5mA LED drive */

/*calibration*/
#define DEFAULT_CROSS_TALK	400
#define ADD_TO_CROSS_TALK	300
#define SUB_FROM_PS_THRESHOLD	100

/*PS tuning value*/
static int apds993x_ps_detection_threshold = 0;
static int apds993x_ps_hsyteresis_threshold = 0;
static int apds993x_ps_pulse_number = 0;
static int apds993x_ps_pgain = 0;

static unsigned int last_ps_state = 0;

#ifdef VENDOR_EDIT /* LiuPing@Phone.BSP.Sensor, 2014/08/04, add for dynamic threshold */
#define APDS993X_ALSPS_DYNAMIC_THRESHOLD
#endif /*VENDOR_EDIT*/
#ifdef APDS993X_ALSPS_DYNAMIC_THRESHOLD

static int ps_min = 0;
static int ps_adjust_max = 850;
static int dirty_adjust_low_thd = 150, dirty_adjust_high_thd = 300;
static int ps_thd_low_highlight = 600, ps_thd_high_highlight = 650;

static struct delayed_work sample_ps_work;
static DECLARE_WAIT_QUEUE_HEAD(enable_wq);

#endif//PROX_ALGO

typedef enum
{
	APDS993X_ALS_RES_10240 = 0,    /* 27.2ms integration time */
	APDS993X_ALS_RES_19456 = 1,    /* 51.68ms integration time */
	APDS993X_ALS_RES_37888 = 2     /* 100.64ms integration time */
} apds993x_als_res_e;

typedef enum
{
	APDS993X_ALS_GAIN_1X    = 0,    /* 1x AGAIN */
	APDS993X_ALS_GAIN_8X    = 1,    /* 8x AGAIN */
	APDS993X_ALS_GAIN_16X   = 2,    /* 16x AGAIN */
	APDS993X_ALS_GAIN_120X  = 3     /* 120x AGAIN */
} apds993x_als_gain_e;

/*
 * Structs
 */
struct apds993x_data {
	struct i2c_client *client;
	struct mutex update_lock;
	struct wake_lock ps_wakelock;
	struct mutex op_mutex;
	struct delayed_work	dwork;		/* for PS interrupt */
	struct delayed_work	als_dwork;	/* for ALS polling */
	struct input_dev *input_dev_als;
	struct input_dev *input_dev_ps;
	struct sensors_classdev als_cdev;
	struct sensors_classdev ps_cdev;

	/* pinctrl data*/
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_sleep;

	struct apds993x_platform_data *platform_data;
	int irq;

	/* regulator data */
	bool power_on;
	struct regulator *vdd;
	struct regulator *vio;

	/* register configuration*/
	unsigned int enable;
	unsigned int atime;
	unsigned int ptime;
	unsigned int wtime;
	unsigned int ailt;
	unsigned int aiht;
	unsigned int pilt;
	unsigned int piht;
	unsigned int pers;
	unsigned int config;
	unsigned int ppcount;
	unsigned int control;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;
	unsigned int enable_als_sensor;

	/* save sensor enabling state for resume */
	unsigned int als_enable_state;

	/* PS parameters */
	unsigned int ps_threshold;
	unsigned int ps_hysteresis_threshold; 	/* always lower than ps_threshold */
	unsigned int ps_detection;		/* 5 = near-to-far; 0 = far-to-near */
	unsigned int ps_data;			/* to store PS data */

	/*calibration*/
	unsigned int cross_talk;		/* cross_talk value */
	unsigned int avg_cross_talk;		/* average cross_talk  */
	unsigned int ps_cal_result;		/* result of calibration*/

	/* ALS parameters */
	unsigned int als_threshold_l;	/* low threshold */
	unsigned int als_threshold_h;	/* high threshold */
	unsigned int als_data;		/* to store ALS data */
	int als_prev_lux;		/* to store previous lux value */

	unsigned int als_gain;		/* needed for Lux calculation */
	unsigned int als_poll_delay;	/* needed for light sensor polling : micro-second (us) */
	unsigned int als_atime_index;	/* storage for als integratiion time */
	unsigned int als_again_index;	/* storage for als GAIN */
	unsigned int als_reduce;	/* flag indicate ALS 6x reduction */
};

static struct sensors_classdev sensors_light_cdev = {
	.name = "apds9930-light",
	.vendor = "avago",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "60000",
	.resolution = "0.0125",
	.sensor_power = "0.20",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev sensors_proximity_cdev = {
	.name = "apds9930-proximity",
	.vendor = "avago",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "3",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

/*
 * Global data
 */
static struct apds993x_data *pdev_data = NULL;

static int g_is_resumed = 1;

/* global i2c_client to support ioctl */
static struct i2c_client *apds993x_i2c_client = NULL;
static struct workqueue_struct *apds993x_workqueue = NULL;

static unsigned char apds993x_als_atime_tb[] = { 0xF6, 0xED, 0xDB };
static unsigned short apds993x_als_integration_tb[] = {2720, 5168, 10064};
static unsigned short apds993x_als_res_tb[] = { 10240, 19456, 37888 };
static unsigned char apds993x_als_again_tb[] = { 1, 8, 16, 120 };
static unsigned char apds993x_als_again_bit_tb[] = { 0x00, 0x01, 0x02, 0x03 };

extern int lcd_dev;
#ifdef VENDOR_EDIT
/* Xiaori.Yuan@Mobile Phone Software Dept.Driver, 2015/01/09  Add for LCD driver */
enum {
	LCD_JDI=0,
	LCD_TRULY,
	LCD_SHARP,
	LCD_HX8394_TRULY,
	LCD_HX8394_TRULY_P3,
	LCD_HX8394_TRULY_P3_MURA,
	LCD_BYD,
	LCD_TM_HX8379,
	LCD_TM_HX8392B,
	LCD_TM_NT35512S,
	LCD_BOE_HX8379S,
	LCD_JDI_NT35521,
	LCD_14045_17_VIDEO,
	LCD_14045_17_CMD,
	LCD_UNKNOW,
}; 
#endif /*VENDOR_EDIT*/


/*calibration*/
static int apds993x_cross_talk_val = 0;

/* ALS tuning */
static int apds993x_ga = 0;
static int apds993x_coe_b = 0;
static int apds993x_coe_c = 0;
static int apds993x_coe_d = 0;

static int apds993x_als_gain = 100;   // the true: lux = (apds993x_als_gain/100)*lux.

static int apds993x_set_als_poll_delay(struct i2c_client *client, unsigned int val);

static int apds993x_sensor_regulator_power_on(struct apds993x_data *data, bool on);
static int apds993x_init_device(struct i2c_client *client);

/*
 * Management functions
 */
static int apds993x_set_command(struct i2c_client *client, int command)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;
	int clearInt;

	if (command == 0)
		clearInt = CMD_CLR_PS_INT;
	else if (command == 1)
		clearInt = CMD_CLR_ALS_INT;
	else
		clearInt = CMD_CLR_PS_ALS_INT;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte(client, clearInt);
	if (ret)
	{
		pr_err("%s: write i2c error, ret=%d\n", __func__, ret);
		mutex_unlock(&data->update_lock);
		return ret;
	}  
	mutex_unlock(&data->update_lock);

	return ret;
}

static int apds993x_set_enable(struct i2c_client *client, int enable)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_ENABLE_REG, enable);
	mutex_unlock(&data->update_lock);

	data->enable = enable;

	return ret;
}

static int apds993x_set_atime(struct i2c_client *client, int atime)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_ATIME_REG, atime);
	mutex_unlock(&data->update_lock);

	data->atime = atime;

	return ret;
}

static int apds993x_set_ptime(struct i2c_client *client, int ptime)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_PTIME_REG, ptime);
	mutex_unlock(&data->update_lock);

	data->ptime = ptime;

	return ret;
}

static int apds993x_set_wtime(struct i2c_client *client, int wtime)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_WTIME_REG, wtime);
	mutex_unlock(&data->update_lock);

	data->wtime = wtime;

	return ret;
}

static int apds993x_set_pilt(struct i2c_client *client, int threshold)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client,CMD_WORD|APDS993X_PILTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->pilt = threshold;

	return ret;
}

static int apds993x_set_piht(struct i2c_client *client, int threshold)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_word_data(client,CMD_WORD|APDS993X_PIHTL_REG, threshold);
	mutex_unlock(&data->update_lock);

	data->piht = threshold;

	return ret;
}

static int apds993x_set_pers(struct i2c_client *client, int pers)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_PERS_REG, pers);
	mutex_unlock(&data->update_lock);

	data->pers = pers;

	return ret;
}

static int apds993x_set_config(struct i2c_client *client, int config)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_CONFIG_REG, config);
	mutex_unlock(&data->update_lock);

	data->config = config;

	return ret;
}

static int apds993x_set_ppcount(struct i2c_client *client, int ppcount)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_PPCOUNT_REG, ppcount);
	mutex_unlock(&data->update_lock);

	data->ppcount = ppcount;

	return ret;
}

static int apds993x_set_control(struct i2c_client *client, int control)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_CONTROL_REG, control);
	mutex_unlock(&data->update_lock);

	data->control = control;

	return ret;
}

/*calibration*/
static void apds993x_swap(int *x, int *y)
{
	int temp = *x;
	*x = *y;
	*y = temp;
}

static int apds993x_run_cross_talk_calibration(struct i2c_client *client)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	unsigned int sum_of_pdata = 0;
	unsigned int temp_pdata[20];
	unsigned int ArySize = 20;
	unsigned int cal_check_flag = 0;
	int i, j;
	pr_info("%s: START proximity sensor calibration\n", __func__);

RECALIBRATION:
	apds993x_set_enable(client, 0x0D);/* Enable PS and Wait */

	for (i = 0; i < 20; i++) {
		mdelay(6);
		mutex_lock(&data->update_lock);
		temp_pdata[i] = i2c_smbus_read_word_data(client,CMD_WORD|APDS993X_PDATAL_REG);
		mutex_unlock(&data->update_lock);
	}

	/* pdata sorting */
	for (i = 0; i < ArySize - 1; i++)
		for (j = i+1; j < ArySize; j++)
			if (temp_pdata[i] > temp_pdata[j])
				apds993x_swap(temp_pdata + i, temp_pdata + j);

	/* calculate the cross-talk using central 10 data */
	for (i = 5; i < 15; i++) {
		pr_info("%s: temp_pdata = %d\n", __func__, temp_pdata[i]);
		sum_of_pdata = sum_of_pdata + temp_pdata[i];
	}

	data->cross_talk = sum_of_pdata/10;
	pr_info("%s: sum_of_pdata = %d   cross_talk = %d\n",__func__, sum_of_pdata, data->cross_talk);

	/*
	 * this value is used at Hidden Menu to check
	 * if the calibration is pass or fail
	 */
	data->avg_cross_talk = data->cross_talk;

	if (data->cross_talk > 720) {
		pr_warn("%s: invalid calibrated data\n", __func__);

		if (cal_check_flag == 0) {
			pr_info("%s: RECALIBRATION start\n", __func__);
			cal_check_flag = 1;
			goto RECALIBRATION;
		} else {
			pr_err("%s: CALIBRATION FAIL - cross_talk is set to DEFAULT\n", __func__);
			data->cross_talk = DEFAULT_CROSS_TALK;
			apds993x_set_enable(client, 0x00); /* Power Off */
			data->ps_cal_result = 0; /* 0:Fail, 1:Pass */
			return -EINVAL;
		}
	}

	data->ps_threshold = ADD_TO_CROSS_TALK + data->cross_talk;
	data->ps_hysteresis_threshold =
		data->ps_threshold - SUB_FROM_PS_THRESHOLD;

	apds993x_set_enable(client, 0x00); /* Power Off */
	data->ps_cal_result = 1;

	pr_info("%s: total_pdata = %d & cross_talk = %d\n",__func__, sum_of_pdata, data->cross_talk);
	pr_info("%s: FINISH proximity sensor calibration\n", __func__);

	/* Save the cross-talk to the non-volitile memory in the phone  */
	return data->cross_talk;
}

static int LuxCalculation(struct i2c_client *client, int ch0data, int ch1data)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int luxValue=0;
	int IAC1=0;
	int IAC2=0;
	int IAC=0;

	if (ch0data >= apds993x_als_res_tb[data->als_atime_index] ||
			ch1data >= apds993x_als_res_tb[data->als_atime_index]) {
		luxValue = data->als_prev_lux;
		return 1999;  // when in high light, maybe get the last invalid value which is low . 
	}

	/* re-adjust COE_B to avoid 2 decimal point */
	IAC1 = (ch0data - (apds993x_coe_b * ch1data) / 100);
	/* re-adjust COE_C and COE_D to void 2 decimal point */
	IAC2 = ((apds993x_coe_c * ch0data) / 100 -
			(apds993x_coe_d * ch1data) / 100);

	if (IAC1 > IAC2)
		IAC = IAC1;
	else if (IAC1 <= IAC2)
		IAC = IAC2;
	else
		IAC = 0;

	if(ch0data != 0 && ch1data*100/ch0data >= 45)      /* LiuPing@Phone.BSP.Sensor, 2014/12/16, when in F illuminant, set the IAC. */
	{
		IAC = ch1data/10;
	}
	else
	{
		if (IAC1 < 0 && IAC2 < 0) {
			IAC = 0;	/* cdata and irdata saturated */
			//return -1; 	/* don't report first, change gain may help */
			//printk(KERN_ERR"%s the unknow light . ch1data:%d ch0data:%d  \n", __func__, ch1data, ch0data); 
			return 2000;   /* LiuPing@Phone.BSP.Sensor, 2014/11/17, when in F illuminant, report the fixed lux. */
		}
	}

	if (data->als_reduce) {
		luxValue = ((IAC * apds993x_ga * APDS993X_DF) / 100) * 65 / 10 /
			((apds993x_als_integration_tb[data->als_atime_index] /
			  100) * apds993x_als_again_tb[data->als_again_index]);
	} else {
		luxValue = ((IAC * apds993x_ga * APDS993X_DF) /100 * 100 / 3) /
			((apds993x_als_integration_tb[data->als_atime_index] /
			  100) * apds993x_als_again_tb[data->als_again_index]);
	}

	return luxValue*apds993x_als_gain/100;
}

static void apds993x_change_ps_threshold(struct i2c_client *client)
{
	int ret = 0;
	struct apds993x_data *data = i2c_get_clientdata(client);

	data->ps_data =	i2c_smbus_read_word_data(client, CMD_WORD|APDS993X_PDATAL_REG);
	if (data->ps_data < 0)
	{
		pr_err("%s: read ps_data=%d  failed !\n",__func__, data->ps_data);
		return;
	}

	if ((data->ps_data > data->pilt) && (data->ps_data >= data->piht)) {
		/* far-to-near detected */
		data->ps_detection = 1;

		/* FAR-to-NEAR detection */
		if (last_ps_state != data->ps_detection)
		{
			pr_err("%s: far-to-near\n", __func__);
			input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);
			input_sync(data->input_dev_ps);
			wake_lock_timeout(&data->ps_wakelock, 2*HZ);
		}
		ret = i2c_smbus_write_word_data(client,CMD_WORD|APDS993X_PILTL_REG,data->pilt);
		if (ret)
		{
			pr_err("%s: write i2c error, ret=%d\n", __func__, ret);
			return;
		}

		ret = i2c_smbus_write_word_data(client,CMD_WORD|APDS993X_PIHTL_REG, 1023);
		if (ret)
		{
			pr_err("%s: write i2c error, ret=%d\n", __func__, ret);
			return;
		}

	} 
	else if ((data->ps_data <= data->pilt) &&(data->ps_data < data->piht)) 
	{
		/* near-to-far detected */
		data->ps_detection = 0;

		/* NEAR-to-FAR detection */
		if (last_ps_state != data->ps_detection)
		{
			pr_err("%s: near-to-far\n", __func__);
			input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);
			input_sync(data->input_dev_ps);
			wake_lock_timeout(&data->ps_wakelock, 2*HZ);
		}
		ret = i2c_smbus_write_word_data(client,CMD_WORD|APDS993X_PILTL_REG, 0);
		if (ret)
		{
			pr_err("%s: write i2c error, ret=%d\n", __func__, ret);
			return;
		}

		ret = i2c_smbus_write_word_data(client,CMD_WORD|APDS993X_PIHTL_REG,data->piht);
		if (ret)
		{
			pr_err("%s: write i2c error, ret=%d\n", __func__, ret);
			return;
		}
	}

	last_ps_state = data->ps_detection;

	pr_err("%s: prox ps_detection = %d, ps_data = %d, th_low = %d, th_high = %d\n", __func__, data->ps_detection, data->ps_data, data->pilt, data->piht);
}

static void apds993x_reschedule_work(struct apds993x_data *data,unsigned long delay)
{
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	cancel_delayed_work(&data->dwork);
	queue_delayed_work(apds993x_workqueue, &data->dwork, delay);
}

static void apds993x_als_polling_work_handler(struct work_struct *work)
{
	struct apds993x_data *data = container_of(work,
			struct apds993x_data, als_dwork.work);
	struct i2c_client *client=data->client;
	int ch0data, ch1data, pdata, v;
	int luxValue=0;
	int err;
	unsigned char change_again=0;
	unsigned char control_data=0;
	unsigned char lux_is_valid=1;

	ch0data = i2c_smbus_read_word_data(client,CMD_WORD|APDS993X_CH0DATAL_REG);
	ch1data = i2c_smbus_read_word_data(client,CMD_WORD|APDS993X_CH1DATAL_REG);
	pdata = i2c_smbus_read_word_data(client,CMD_WORD|APDS993X_PDATAL_REG);

	luxValue = LuxCalculation(client, ch0data, ch1data);

	if (luxValue >= 0) {
		luxValue = (luxValue < ALS_MAX_RANGE)? luxValue : ALS_MAX_RANGE;
		data->als_prev_lux = luxValue;
	} else {
		/* don't report, this is invalid lux value */
		lux_is_valid = 0;
		luxValue = data->als_prev_lux;
		if (data->als_reduce)
			lux_is_valid = 1;
		/* report anyway since this is the lowest gain */
	}

	/*
	 * check PS under sunlight
	 * PS was previously in far-to-near condition
	 */
	v = (75 * (1024 * (256 - data->atime))) / 100;

	if (lux_is_valid) {
		/* report the lux level */
		if (is_project(OPPO_14037) || is_project(OPPO_15057)){
			if(lcd_dev == LCD_HX8394_TRULY_P3_MURA){
				luxValue = (luxValue*86)/100;
			}else if(lcd_dev == LCD_JDI_NT35521){
				luxValue = (luxValue*83)/100;
			}else if(lcd_dev == LCD_TM_HX8392B){
				luxValue = (luxValue*22)/10;
			}
		}
		else if (is_project(OPPO_15005))
			luxValue = (luxValue * 6)/10;

		input_report_abs(data->input_dev_als, ABS_MISC, luxValue);
		input_sync(data->input_dev_als);
	}

	data->als_data = ch0data;

	if (data->als_data >=(apds993x_als_res_tb[data->als_atime_index]* 90) / 100) {
		/* lower AGAIN if possible */
		if (data->als_again_index != APDS993X_ALS_GAIN_1X) {
			data->als_again_index--;
			change_again = 1;
		} else {
			err = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_CONFIG_REG,APDS993X_ALS_REDUCE);
			if (err >= 0)
				data->als_reduce = 1;
		}
	} else if (data->als_data <=
			(apds993x_als_res_tb[data->als_atime_index] * 10) / 100) {
		/* increase AGAIN if possible */
		if (data->als_reduce) {
			err = i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_CONFIG_REG, 0);
			if (err >= 0)
				data->als_reduce = 0;
		} else if (data->als_again_index != APDS993X_ALS_GAIN_120X) {
			data->als_again_index++;
			change_again = 1;
		}
	}

	if (change_again) {
		control_data = i2c_smbus_read_byte_data(client,CMD_BYTE|APDS993X_CONTROL_REG);
		control_data = control_data & 0xFC;
		control_data = control_data |apds993x_als_again_bit_tb[data->als_again_index];
		i2c_smbus_write_byte_data(client,CMD_BYTE|APDS993X_CONTROL_REG, control_data);
	}

	/* restart timer */
	queue_delayed_work(apds993x_workqueue,&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));
}

/* PS interrupt routine */
static void apds993x_work_handler(struct work_struct *work)
{
	struct apds993x_data *data =container_of(work, struct apds993x_data, dwork.work);
	struct i2c_client *client=data->client;
	int status;
	int enable;

	status = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS993X_STATUS_REG);
	enable = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS993X_ENABLE_REG);

	if (status < 0 || enable < 0)
	{
		pr_err("%s: i2c error , status = %d  enable = %d \n", __func__, status, enable);
		goto exit;
	}

	if ((status & enable & 0x20) == 0x20) 
	{
		apds993x_change_ps_threshold(client);

		apds993x_set_command(client, 0);
	} 

	if (data->irq)
	{
		enable_irq(data->irq);
	}

	return;

exit:
	apds993x_set_command(client, 2);  
	msleep(30);
	if (data->irq)
	{
		enable_irq(data->irq);
	}
	return;
}

/* assume this is ISR */
static irqreturn_t apds993x_interrupt(int vec, void *info)
{
	struct i2c_client *client=(struct i2c_client *)info;
	struct apds993x_data *data = i2c_get_clientdata(client);
	printk("%s occur... \n", __func__);
	disable_irq_nosync(data->irq);
	apds993x_reschedule_work(data, 0);

	return IRQ_HANDLED;
}

/*
 * IOCTL support
 */
static int apds993x_enable_als_sensor(struct i2c_client *client, int val)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	struct apds993x_platform_data *pdata = data->platform_data;
	int rc;

	pr_err("%s: val=%d\n", __func__, val);

	if ((val != 0) && (val != 1)) {
		pr_err("%s: invalid value (val = %d)\n", __func__, val);
		return -EINVAL;
	}

	mutex_lock(&data->op_mutex);
	if (val == 1) {
		/* turn on light  sensor */
		if ((data->enable_als_sensor == 0) &&(data->enable_ps_sensor == 0)) {
			/* Power on and initalize the device */
			if (pdata->power_on)
				pdata->power_on(true);

			rc = apds993x_init_device(client);
			if (rc) {
				dev_err(&client->dev, "Failed to init apds993x\n");
				mutex_unlock(&data->op_mutex);
				return rc;
			}
		}

		if (data->enable_als_sensor == 0) {
			data->enable_als_sensor = 1;
			/* Power Off */
			apds993x_set_enable(client,0);

			if (data->enable_ps_sensor) {
				/* Enable PS with interrupt */
				apds993x_set_enable(client, 0x27);
			} else {
				/* no interrupt*/
				apds993x_set_enable(client, 0x03);
			}

			/*
			 * If work is already scheduled then subsequent
			 * schedules will not change the scheduled time
			 * that's why we have to cancel it first.
			 */
			cancel_delayed_work_sync(&data->als_dwork);
			queue_delayed_work(apds993x_workqueue, &data->als_dwork, msecs_to_jiffies(200));  // 200ms > atime + ptime + wtime.
		}
	} else {
		/*
		 * turn off light sensor
		 * what if the p sensor is active?
		 */
		data->enable_als_sensor = 0;

		if (data->enable_ps_sensor) {
			/* Power Off */
			apds993x_set_enable(client,0);

			apds993x_set_piht(client, 0);
			apds993x_set_piht(client,apds993x_ps_detection_threshold);

			/* only enable prox sensor with interrupt */
			apds993x_set_enable(client, 0x27);
		} else {
			apds993x_set_enable(client, 0);
		}

		/*
		 * If work is already scheduled then subsequent schedules
		 * will not change the scheduled time that's why we have
		 * to cancel it first.
		 */
		cancel_delayed_work_sync(&data->als_dwork);
	}

	/* Vote off  regulators if both light and prox sensor are off */
	if ((data->enable_als_sensor == 0) &&(data->enable_ps_sensor == 0) &&(pdata->power_on))
		pdata->power_on(false);

	mutex_unlock(&data->op_mutex);
	return 0;
}

static int apds993x_set_als_poll_delay(struct i2c_client *client,unsigned int val)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret;
	int atime_index=0;

	pr_debug("%s: val=%d\n", __func__, val);
	mutex_lock(&data->op_mutex);

	/* minimum 3ms */
	if (val < 3)
		val = 3;
	data->als_poll_delay = val;

	if (data->als_poll_delay >= 100)
		atime_index = APDS993X_ALS_RES_37888;
	else if (data->als_poll_delay >= 50)
		atime_index = APDS993X_ALS_RES_19456;
	else
		atime_index = APDS993X_ALS_RES_10240;

	ret = apds993x_set_atime(client, apds993x_als_atime_tb[atime_index]);
	if (ret >= 0) {
		data->als_atime_index = atime_index;
		pr_debug("poll delay %d, atime_index %d\n",data->als_poll_delay, data->als_atime_index);
	} else {
		mutex_unlock(&data->op_mutex);
		return ret;
	}

	if (data->enable_als_sensor) {
		mod_delayed_work(apds993x_workqueue,&data->als_dwork,msecs_to_jiffies(data->als_poll_delay));
	}

	mutex_unlock(&data->op_mutex);

	return 0;
}

static int apds993x_enable_ps_sensor(struct i2c_client *client, int val)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	struct apds993x_platform_data *pdata = data->platform_data;
	int rc;
	uint16_t ps;

	pr_err("%s: val=%d\n", __func__, val);

	if ((val != 0) && (val != 1)) {
		pr_err("%s: invalid value=%d\n", __func__, val);
		return -EINVAL;
	}

	data->ps_detection = 0;

	if (val == 1) {
		/* turn on p sensor */
		if ((data->enable_als_sensor == 0) &&
				(data->enable_ps_sensor == 0)) {
			/* Power on and initalize the device */
			if (pdata->power_on)
				pdata->power_on(true);

			rc = apds993x_init_device(client);
			if (rc) {
				dev_err(&client->dev, "Failed to init apds993x\n");
				return rc;
			}
		}

		if (data->enable_ps_sensor==0) {
			data->enable_ps_sensor= 1;

			/* Power Off */
			apds993x_set_enable(client,0);

			/* init threshold for proximity */
			/*calirbation*/
			//		apds993x_set_ps_threshold_adding_cross_talk(client, data->cross_talk);

			apds993x_set_enable(client, 0x27);
			msleep(50);

#ifdef APDS993X_ALSPS_DYNAMIC_THRESHOLD
			if (ps_min != 0 && ps_min + dirty_adjust_high_thd < ps_adjust_max){
				apds993x_ps_detection_threshold = ps_min + dirty_adjust_low_thd;
				apds993x_set_pilt(client, apds993x_ps_detection_threshold);

				apds993x_ps_detection_threshold = ps_min + dirty_adjust_high_thd;
				apds993x_set_piht(client, apds993x_ps_detection_threshold);
			}
			else
			{
				apds993x_ps_detection_threshold = ps_thd_high_highlight;
				apds993x_set_pilt(client, ps_thd_low_highlight);
				apds993x_set_piht(client, ps_thd_high_highlight);
			}
#endif

			ps = i2c_smbus_read_word_data(client,CMD_WORD|APDS993X_PDATAL_REG);
			printk(KERN_ERR"%s:ps=%d  low=%d  high=%d\n", __func__, ps, data->pilt, data->piht);
			if (ps >= data->piht)
				data->ps_detection = 1;
			else if (ps <= data->pilt)
				data->ps_detection = 0;

			last_ps_state = data->ps_detection;
			printk(KERN_ERR"%s:ps original state is : %d %s\n",__func__,data->ps_detection, data->ps_detection?"near":"far");
			input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_detection ? 0:1);
			input_sync(data->input_dev_ps);
			wake_lock_timeout(&data->ps_wakelock, 2*HZ);

			if (data->enable_als_sensor==0) {
				/* only enable PS interrupt */
				if (data->irq) {
					enable_irq(data->irq);
					irq_set_irq_wake(client->irq, 1);
				}
			} else {
				/* enable PS interrupt */
				if (data->irq) {
					enable_irq(data->irq);
					irq_set_irq_wake(client->irq, 1);
				}
			}
		}
#ifdef APDS993X_ALSPS_DYNAMIC_THRESHOLD
		wake_up(&enable_wq);	
#endif
	} else {
		/*
		 * turn off p sensor - kk 25 Apr 2011
		 * we can't turn off the entire sensor,
		 * the light sensor may be needed by HAL
		 */
		data->enable_ps_sensor = 0;
		if (data->enable_als_sensor) {
			/* no ALS interrupt */
			if (data->irq) {
				irq_set_irq_wake(client->irq, 0);
				disable_irq(data->irq);
			}

			apds993x_set_enable(client, 0x03);

			/*
			 * If work is already scheduled then subsequent
			 * schedules will not change the scheduled time
			 * that's why we have to cancel it first.
			 */
			cancel_delayed_work_sync(&data->als_dwork);
			/* 100ms */
			queue_delayed_work(apds993x_workqueue,&data->als_dwork,msecs_to_jiffies(data->als_poll_delay));
		} else {
			if (data->irq) {
				irq_set_irq_wake(client->irq, 0);
				disable_irq(data->irq);
			}
			apds993x_set_enable(client, 0);
			/*
			 * If work is already scheduled then subsequent
			 * schedules will not change the scheduled time
			 * that's why we have to cancel it first.
			 */
			cancel_delayed_work_sync(&data->als_dwork);
		}
	}

	/* Vote off  regulators if both light and prox sensor are off */
	if ((data->enable_als_sensor == 0) &&(data->enable_ps_sensor == 0) &&(pdata->power_on))
		pdata->power_on(false);

	return 0;
}

/*
 * SysFS support
 */
static ssize_t apds993x_show_ch0data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ch0data;

	mutex_lock(&data->update_lock);
	ch0data = i2c_smbus_read_word_data(client,
			CMD_WORD|APDS993X_CH0DATAL_REG);
	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", ch0data);
}

static DEVICE_ATTR(ch0data, S_IRUGO, apds993x_show_ch0data, NULL);

static ssize_t apds993x_show_ch1data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ch1data;

	mutex_lock(&data->update_lock);
	ch1data = i2c_smbus_read_word_data(client,
			CMD_WORD|APDS993X_CH1DATAL_REG);
	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", ch1data);
}

static DEVICE_ATTR(ch1data, S_IRUGO, apds993x_show_ch1data, NULL);

static ssize_t apds993x_show_pdata(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);
	int pdata;

	mutex_lock(&data->update_lock);
	pdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS993X_PDATAL_REG);
	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", pdata);
}

static DEVICE_ATTR(pdata, S_IRUGO, apds993x_show_pdata, NULL);

/*calibration sysfs*/
static ssize_t apds993x_show_status(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);
	int status;
	int rdata;

	mutex_lock(&data->update_lock);
	status = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS993X_STATUS_REG);
	rdata = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS993X_ENABLE_REG);
	mutex_unlock(&data->update_lock);

	pr_info("%s: APDS993x_ENABLE_REG = %2d,  APDS993x_STATUS_REG = %2d\n",__func__, rdata, status);

	return sprintf(buf, "%d\n", status);
}

static DEVICE_ATTR(status, S_IRUSR | S_IRGRP, apds993x_show_status, NULL);

static ssize_t apds993x_show_ps_run_calibration(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", data->avg_cross_talk);
}

static ssize_t apds993x_store_ps_run_calibration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);
	int ret = 0;

	/* start calibration */
	ret = apds993x_run_cross_talk_calibration(client);

	/* set threshold for near/far status */
	data->ps_threshold = data->cross_talk + ADD_TO_CROSS_TALK;
	data->ps_hysteresis_threshold = data->ps_threshold - SUB_FROM_PS_THRESHOLD;

	pr_info("%s: [piht][pilt][c_t] = [%d][%d][%d]\n", __func__, data->ps_threshold,data->ps_hysteresis_threshold,data->cross_talk);

	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(ps_run_calibration,  S_IWUSR | S_IWGRP | S_IRUGO,
		apds993x_show_ps_run_calibration,apds993x_store_ps_run_calibration);

static ssize_t apds993x_show_ps_default_crosstalk(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", DEFAULT_CROSS_TALK);
}

static ssize_t apds993x_store_ps_default_crosstalk(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);

	data->ps_threshold = DEFAULT_CROSS_TALK + ADD_TO_CROSS_TALK;
	data->ps_hysteresis_threshold = data->ps_threshold - SUB_FROM_PS_THRESHOLD;

	pr_info("%s: [piht][pilt][c_t] = [%d][%d][%d]\n", __func__,
			data->ps_threshold,
			data->ps_hysteresis_threshold,
			data->cross_talk);

	return count;
}

static DEVICE_ATTR(ps_default_crosstalk, S_IRUGO | S_IWUSR | S_IWGRP,
		apds993x_show_ps_default_crosstalk,apds993x_store_ps_default_crosstalk);

/* for Calibration result */
static ssize_t apds993x_show_ps_cal_result(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", data->ps_cal_result);
}

static DEVICE_ATTR(ps_cal_result, S_IRUGO, apds993x_show_ps_cal_result, NULL);
/*calibration sysfs end*/

static ssize_t apds993x_show_enable_ps_sensor(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", data->enable_ps_sensor);
}

static ssize_t apds993x_store_enable_ps_sensor(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	pr_debug("%s: val=%ld\n", __func__, val);

	if (val != 0 && val != 1) {
		pr_err("%s: invalid value(%ld)\n", __func__, val);
		return -EINVAL;
	}

	apds993x_enable_ps_sensor(client, val);

	return count;
}

static DEVICE_ATTR(enable_ps_sensor, S_IWUSR | S_IWGRP | S_IRUGO,
		apds993x_show_enable_ps_sensor,
		apds993x_store_enable_ps_sensor);

static ssize_t apds993x_show_enable_als_sensor(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", data->enable_als_sensor);
}

static ssize_t apds993x_store_enable_als_sensor(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	pr_debug("%s: val=%ld\n", __func__, val);

	if (val != 0 && val != 1) {
		pr_err("%s: invalid value(%ld)\n", __func__, val);
		return -EINVAL;
	}

	apds993x_enable_als_sensor(client, val);

	return count;
}

static int apds993x_als_set_enable(struct sensors_classdev *sensors_cdev,unsigned int enable)
{
	struct apds993x_data *data = container_of(sensors_cdev,
			struct apds993x_data, als_cdev);

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	return apds993x_enable_als_sensor(data->client, enable);
}

static int apds993x_ps_set_enable(struct sensors_classdev *sensors_cdev,unsigned int enable)
{
	struct apds993x_data *data = container_of(sensors_cdev,
			struct apds993x_data, ps_cdev);

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	return apds993x_enable_ps_sensor(data->client, enable);
}

static DEVICE_ATTR(enable_als_sensor, S_IWUSR | S_IWGRP | S_IRUGO,
		apds993x_show_enable_als_sensor,
		apds993x_store_enable_als_sensor);

static ssize_t apds993x_show_als_poll_delay(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds993x_data *data = i2c_get_clientdata(client);

	/* return in micro-second */
	return snprintf(buf, PAGE_SIZE, "%d\n", data->als_poll_delay);
}

static ssize_t apds993x_store_als_poll_delay(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	apds993x_set_als_poll_delay(client, val);

	return count;
}

static int apds993x_als_poll_delay(struct sensors_classdev *sensors_cdev,unsigned int delay_msec)
{
	struct apds993x_data *data = container_of(sensors_cdev,
			struct apds993x_data, als_cdev);
	apds993x_set_als_poll_delay(data->client, delay_msec);
	return 0;
}

static DEVICE_ATTR(als_poll_delay, S_IWUSR | S_IRUGO,apds993x_show_als_poll_delay, apds993x_store_als_poll_delay);


static struct attribute *apds993x_attributes[] = {
	&dev_attr_ch0data.attr,
	&dev_attr_ch1data.attr,
	&dev_attr_pdata.attr,
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_als_sensor.attr,
	&dev_attr_als_poll_delay.attr,
	/*calibration*/
	&dev_attr_status.attr,
	&dev_attr_ps_run_calibration.attr,
	&dev_attr_ps_default_crosstalk.attr,
	&dev_attr_ps_cal_result.attr,
	NULL
};

static const struct attribute_group apds993x_attr_group = {
	.attrs = apds993x_attributes,
};

static int apds993x_check_chip_id(struct i2c_client *client)
{
	int id;

	id = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS993X_ID_REG);
	switch (id) {
		case APDS9931_ID:
			dev_dbg(&client->dev, "APDS9931\n");
			break;

		case APDS9930_ID:
			dev_dbg(&client->dev, "APDS9930\n");
			break;

		case APDS9900_ID:
			dev_dbg(&client->dev, "APDS9900\n");
			break;

		case APDS9901_ID:
			dev_dbg(&client->dev, "APDS9931\n");
			break;
		default:
			dev_err(&client->dev, "Neither APDS993x nor APDS990x\n");
			return -ENODEV;
	}
	return 0;
}

/*
 * Initialization function
 */
static int apds993x_init_device(struct i2c_client *client)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	int err;

	err = apds993x_set_enable(client, 0);
	if (err < 0)
		return err;

	/* 100.64ms ALS integration time */
	err = apds993x_set_atime(client,apds993x_als_atime_tb[data->als_atime_index]);
	if (err < 0)
		return err;

	/* 2.72ms Prox integration time */
	err = apds993x_set_ptime(client, 0xFF);
	if (err < 0)
		return err;

	/* 2.72ms Wait time */
	err = apds993x_set_wtime(client, 0xFF);
	if (err < 0)
		return err;

	err = apds993x_set_ppcount(client, apds993x_ps_pulse_number);
	if (err < 0)
		return err;

	/* no long wait */
	err = apds993x_set_config(client, 0);
	if (err < 0)
		return err;

	err = apds993x_set_control(client,
			APDS993X_PDRVIE_100MA |APDS993X_PRX_IR_DIOD |
			apds993x_ps_pgain |apds993x_als_again_bit_tb[data->als_again_index]);
	if (err < 0)
		return err;

	/* init threshold for proximity */
	/*
	   err = apds993x_set_pilt(client, 0);
	   if (err < 0)
	   return err;

	   err = apds993x_set_piht(client, apds993x_ps_detection_threshold);
	   if (err < 0)
	   return err;
	 */
	/*calirbation*/
	//apds993x_set_ps_threshold_adding_cross_talk(client, data->cross_talk);
	data->ps_detection = 0; /* initial value = far*/

	/* 2 consecutive Interrupt persistence */
	err = apds993x_set_pers(client, APDS993X_PPERS_2|APDS993X_APERS_1);
	if (err < 0)
		return err;

	/* sensor is in disabled mode but all the configurations are preset */
	return 0;
}

static int apds993x_suspend(struct device *dev)
{
	struct apds993x_data *data;
	struct apds993x_platform_data *pdata;
	int rc;
	printk("%s enter \n", __func__);
	data = dev_get_drvdata(dev);
	pdata = data->platform_data;

	/*
	 * Save sensor state and disable them,
	 * this is to ensure internal state flags are set correctly.
	 * device will power off after both sensors are disabled.
	 * P sensor will not be disabled because it  is a wakeup sensor.
	 */
	data->als_enable_state = data->enable_als_sensor;

	if (data->als_enable_state) {
		rc = apds993x_enable_als_sensor(data->client, 0);
		if (rc)
			dev_err(&data->client->dev,
					"Disable light sensor fail! rc=%d\n", rc);
	}
	if (data->irq && data->enable_ps_sensor) 
	{
		disable_irq(data->irq);    
	}

	g_is_resumed = 0;

	return 0;
}

static int apds993x_resume(struct device *dev)
{
	struct apds993x_data *data;
	struct apds993x_platform_data *pdata;
	int rc;
	printk("%s enter \n", __func__);
	data = dev_get_drvdata(dev);
	pdata = data->platform_data;

	/* Resume L sensor state as P sensor does not disable */
	if (data->als_enable_state) {
		rc = apds993x_enable_als_sensor(data->client, 1);
		if (rc)
			dev_err(&data->client->dev,"Disable light sensor fail! rc=%d\n", rc);
	}
	if (data->irq && data->enable_ps_sensor) 
	{
		enable_irq(data->irq);
	}

	g_is_resumed = 1;

	return 0;
}

static int apds993x_sensor_regulator_configure(struct apds993x_data *data, bool on)
{
	int rc;

	printk(KERN_ERR"%s, on = %d\n", __func__, on);

	if (!on) {

		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0,APDS993X_VDD_MAX_UV);

		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0,APDS993X_VIO_MAX_UV);

		regulator_put(data->vio);
	} else {
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			rc = PTR_ERR(data->vdd);
			dev_err(&data->client->dev,"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			rc = regulator_set_voltage(data->vdd,
					APDS993X_VDD_MIN_UV, APDS993X_VDD_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,"Regulator set failed vdd rc=%d\n",rc);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) {
			rc = PTR_ERR(data->vio);
			dev_err(&data->client->dev,"Regulator get failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0) {
			rc = regulator_set_voltage(data->vio,APDS993X_VIO_MIN_UV, APDS993X_VIO_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev,"Regulator set failed vio rc=%d\n", rc);
				goto reg_vio_put;
			}
		}
	}

	return 0;
reg_vio_put:
	regulator_put(data->vio);

reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, APDS993X_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
}

static int apds993x_sensor_regulator_power_on(struct apds993x_data *data, bool on)
{
	int rc = 0;

	printk(KERN_ERR"%s, on = %d\n", __func__, on);

	if (!on) {
		rc = regulator_disable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(data->vio);
		if (rc) {
			dev_err(&data->client->dev,"Regulator vio disable failed rc=%d\n", rc);
			rc = regulator_enable(data->vdd);
			dev_err(&data->client->dev,"Regulator vio re-enabled rc=%d\n", rc);
			/*
			 * Successfully re-enable regulator.
			 * Enter poweron delay and returns error.
			 */
			if (!rc) {
				rc = -EBUSY;
				goto enable_delay;
			}
		}
		return rc;
	} else {
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(data->vio);
		if (rc) {
			dev_err(&data->client->dev,"Regulator vio enable failed rc=%d\n", rc);
			regulator_disable(data->vdd);
			return rc;
		}
	}

enable_delay:
	msleep(20);
	dev_dbg(&data->client->dev,"Sensor regulator power on =%d\n", on);
	return rc;
}

static int sensor_platform_hw_power_on(bool on)
{
	struct apds993x_data *data;
	int err = 0;

	if (pdev_data == NULL)
		return -ENODEV;

	data = pdev_data;
	if (data->power_on != on) {
		if (!IS_ERR_OR_NULL(data->pinctrl)) {
			if (on)
				err = pinctrl_select_state(data->pinctrl,data->pin_default);
			else
				err = pinctrl_select_state(data->pinctrl,data->pin_sleep);
			if (err)
				dev_err(&data->client->dev,"Can't select pinctrl state\n");
		}

		err = apds993x_sensor_regulator_power_on(data, on);
		if (err)
			dev_err(&data->client->dev,"Can't configure regulator!\n");
		else
			data->power_on = on;
	}

	return err;
}

static int sensor_platform_hw_init(void)
{
	struct i2c_client *client;
	struct apds993x_data *data;
	int error;

	if (pdev_data == NULL)
		return -ENODEV;

	data = pdev_data;
	client = data->client;

	error = apds993x_sensor_regulator_configure(data, true);
	if (error < 0) {
		dev_err(&client->dev, "unable to configure regulator\n");
		return error;
	}

	if (gpio_is_valid(data->platform_data->irq_gpio)) 
	{
		/* configure apds993x irq gpio */
		error = gpio_request_one(data->platform_data->irq_gpio,GPIOF_DIR_IN,"apds993x_irq_gpio");
		if (error) {
			dev_err(&client->dev, "unable to request gpio %d\n",data->platform_data->irq_gpio);
		}
		data->irq = client->irq = gpio_to_irq(data->platform_data->irq_gpio);

		error = gpio_direction_input(data->platform_data->irq_gpio);
		if(error < 0)
		{
			printk(KERN_ERR "%s: gpio_direction_input, err=%d", __func__, error);
			return error;
		}
	} 
	else 
	{
		dev_err(&client->dev, "irq gpio not provided\n");
	}
	return 0;
}

static void sensor_platform_hw_exit(void)
{
	struct apds993x_data *data = pdev_data;

	if (data == NULL)
		return;

	apds993x_sensor_regulator_configure(data, false);

	if (gpio_is_valid(data->platform_data->irq_gpio))
		gpio_free(data->platform_data->irq_gpio);
}

static int apds993x_pinctrl_init(struct apds993x_data *data)
{
	struct i2c_client *client = data->client;

	data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(data->pinctrl)) {dev_err(&client->dev, "Failed to get pinctrl\n");
		return PTR_ERR(data->pinctrl);
	}

	data->pin_default =pinctrl_lookup_state(data->pinctrl, "default");
	if (IS_ERR_OR_NULL(data->pin_default)) {
		dev_err(&client->dev, "Failed to look up default state\n");
		return PTR_ERR(data->pin_default);
	}

	data->pin_sleep =pinctrl_lookup_state(data->pinctrl, "sleep");
	if (IS_ERR_OR_NULL(data->pin_sleep)) {
		dev_err(&client->dev, "Failed to look up sleep state\n");
		return PTR_ERR(data->pin_sleep);
	}

	return 0;
}

static int sensor_parse_dt(struct device *dev,
		struct apds993x_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	unsigned int tmp;
	int rc = 0;

	/* set functions of platform data */
	pdata->init = sensor_platform_hw_init;
	pdata->exit = sensor_platform_hw_exit;
	pdata->power_on = sensor_platform_hw_power_on;

	/* LiuPing@Phone.BSP.Sensor, 2014/09/18, add for set gpio to control ex-ldo for vdd supply in 14005. */
	{
		int vdd_gpio = 0;
		vdd_gpio = of_get_named_gpio(np, "sensor,vdd-gpio", 0);
		if (gpio_is_valid(vdd_gpio)) 
		{
			printk("%s set gpio:%d to high for vdd supply. \n", __func__, vdd_gpio);
			gpio_request(vdd_gpio,"vdd-gpio");
			gpio_direction_output(vdd_gpio, 1);
		}
	}

	/* irq gpio */
	rc = of_get_named_gpio_flags(dev->of_node,"avago,irq-gpio", 0, NULL);
	if (rc < 0) {
		dev_err(dev, "Unable to read irq gpio\n");
		return rc;
	}
	pdata->irq_gpio = rc;

	/* ps tuning data*/
	rc = of_property_read_u32(np, "avago,ps-threshold", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read ps threshold\n");
		return rc;
	}
	pdata->prox_threshold = tmp;

	rc = of_property_read_u32(np, "avago,ps-hysteresis-threshold", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read ps hysteresis threshold\n");
		return rc;
	}
	pdata->prox_hsyteresis_threshold = tmp;

	rc = of_property_read_u32(np, "avago,ps-pulse", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read ps pulse\n");
		return rc;
	}
	pdata->prox_pulse = tmp;

	rc = of_property_read_u32(np, "avago,ps-pgain", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read ps pgain\n");
		return rc;
	}
	pdata->prox_gain = tmp;

	/* ALS tuning value */
	rc = of_property_read_u32(np, "avago,als-B", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read apds993x coefficient b\n");
		return rc;
	}
	pdata->als_B = tmp;

	rc = of_property_read_u32(np, "avago,als-C", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read apds993x coefficient c\n");
		return rc;
	}
	pdata->als_C = tmp;

	rc = of_property_read_u32(np, "avago,als-D", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read apds993x coefficient d\n");
		return rc;
	}
	pdata->als_D = tmp;

	rc = of_property_read_u32(np, "avago,ga-value", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read gain value\n");
		return rc;
	}
	pdata->ga_value = tmp;

	rc = of_property_read_u32(np, "avago,als-gain", &tmp);
	if (rc) 
	{
		apds993x_als_gain = 100;                
	}
	else
	{
		apds993x_als_gain = tmp;
	}

	return 0;
}

/*--------------------------------------------------------------------------*/
#ifdef APDS993X_ALSPS_DYNAMIC_THRESHOLD
static void sample_work_func(struct work_struct *work)
{
	struct apds993x_data *data;
	struct apds993x_platform_data *pdata;
	struct i2c_client *client;
	int ret;
	int i;
	uint16_t ps, als;

	if (apds993x_i2c_client != NULL)
		client = apds993x_i2c_client;
	else{
		printk(KERN_EMERG"%s:apds993x: sample fail because of global pointer is NULL\n",__func__);
		return;
	}
	data = i2c_get_clientdata(apds993x_i2c_client);
	if (data == NULL)
		return;
	pdata = data->platform_data;

	if ((data->enable_als_sensor == 0) && (data->enable_ps_sensor == 0)) {
		/* Power on and initalize the device */
		if (pdata->power_on)
			pdata->power_on(true);

		ret = apds993x_init_device(client);
		if (ret) {
			dev_err(&client->dev, "Failed to init apds993x\n");
			return;
		}
	}

	if (data->enable_ps_sensor==0) {
		apds993x_set_enable(client, 0x07);
	}

	msleep(10);
	for (i = 0; i < 10; i++){
		als = i2c_smbus_read_word_data(client, CMD_WORD|APDS993X_CH0DATAL_REG);
		if (als < 8000){
			ps = i2c_smbus_read_word_data(client,CMD_WORD|APDS993X_PDATAL_REG);

			if( (ps != 0) &&((ps_min == 0) || (ps_min > ps)))
				ps_min = ps;
		}
		msleep(10);
	}

	if (ps_min > ps_adjust_max)
		ps_min = 0;

	if (data->enable_ps_sensor==0) {
		apds993x_set_enable(client, 0x0);
	}

	if ((data->enable_als_sensor == 0) && (data->enable_ps_sensor == 0) && (pdata->power_on))
		pdata->power_on(false);

	pr_err("%s ps_min:%d \n", __func__,  ps_min);
}
#endif
/*--------------------------------------------------------------------------*/
static ssize_t apds993x_alsps_enable_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 data[2];
	int ret = -EINVAL;
	sscanf(buf, "%x %x", (unsigned int *)&data[0],(unsigned int *)&data[1]);
	switch(data[0])
	{
		case SENSOR_TYPE_LIGHT:
			ret = apds993x_enable_als_sensor(apds993x_i2c_client,(int)data[1]);
			if (ret < 0)
			{
				printk("%s: %s als fail\n",__func__, (data[1] == 1)?"enable":"disable");
			}
			break;

		case SENSOR_TYPE_PROXIMITY:
			ret = apds993x_enable_ps_sensor(apds993x_i2c_client, (int)data[1]);
			if (ret < 0)
			{
				printk("%s: %s prox fail\n",__func__, (data[1] == 1)?"enable":"disable");
			}
			break;

		default:
			ret = -EINVAL;
			printk("%s: DO NOT support this type sensor\n",__func__);
			break;
	}
	if (ret == 0)
		printk("%s: Enable sensor SUCCESS\n",__func__);

	return count;
}
static ssize_t apds993x_alsps_enable_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "als:%d  prox:%d\n", pdev_data->enable_als_sensor, pdev_data->enable_ps_sensor);
}
static struct kobj_attribute enable = 
{
	.attr = {"enable", 0664},
	.show = apds993x_alsps_enable_show,
	.store = apds993x_alsps_enable_store,
};
static ssize_t apds993x_prox_raw_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	uint16_t ps_data;
	ps_data =  i2c_smbus_read_word_data(apds993x_i2c_client, CMD_WORD|APDS993X_PDATAL_REG);

	return snprintf(buf, PAGE_SIZE, "%d\n", ps_data);
}
static struct kobj_attribute prox_raw = 
{
	.attr = {"prox_raw", 0444},
	.show = apds993x_prox_raw_show,
};
static ssize_t apds993x_als_raw_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ch0data, ch1data, pdata;
	int luxValue=0;

	ch0data = i2c_smbus_read_word_data(apds993x_i2c_client,CMD_WORD|APDS993X_CH0DATAL_REG);
	ch1data = i2c_smbus_read_word_data(apds993x_i2c_client,CMD_WORD|APDS993X_CH1DATAL_REG);
	pdata = i2c_smbus_read_word_data(apds993x_i2c_client,CMD_WORD|APDS993X_PDATAL_REG);

	luxValue = LuxCalculation(apds993x_i2c_client, ch0data, ch1data);

	return snprintf(buf, PAGE_SIZE, "%d\n", luxValue);
}
static struct kobj_attribute als_raw = 
{
	.attr = {"als_raw", 0444},
	.show = apds993x_als_raw_show,
};
#ifdef APDS993X_ALSPS_DYNAMIC_THRESHOLD    // Do not modify it as wilful, because it is used for algo.
static ssize_t apds993x_name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s", "apds993x");
}
static struct kobj_attribute name = 
{
	.attr = {"name", 0444},
	.show = apds993x_name_show,
};
static ssize_t apds993x_high_light_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ch0data = i2c_smbus_read_word_data(apds993x_i2c_client,CMD_WORD|APDS993X_CH0DATAL_REG);
	return snprintf(buf, PAGE_SIZE, "%d\n",  (ch0data < 8000)? 0:1);
}
static struct kobj_attribute is_high_light = 
{
	.attr = {"is_high_light", 0444},
	.show = apds993x_high_light_show,
};
static ssize_t apds993x_ps_enable_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct apds993x_data *data = i2c_get_clientdata(apds993x_i2c_client);
	return snprintf(buf, PAGE_SIZE, "%d", data->enable_ps_sensor);
}
static struct kobj_attribute ps_enable = 
{
	.attr = {"ps_enable", 0444},
	.show = apds993x_ps_enable_show,
};
static ssize_t apds993x_alsps_ps_thd_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	u32 data[2];
	struct apds993x_data *p_data = i2c_get_clientdata(apds993x_i2c_client);
	if (sscanf(buf, "%d %d", (unsigned int *)&data[0],(unsigned int *)&data[1]) == 2)
	{
		//printk("algo set --- low_thd:%5d, high_thd:%5d \n", data[0], data[1]);

		p_data->piht = data[1];
		p_data->pilt = data[0];
		if(p_data->ps_detection == 0)
		{
			ret = i2c_smbus_write_word_data(apds993x_i2c_client,
					CMD_WORD|APDS993X_PILTL_REG, 0);
			if (ret)
			{
				pr_err("%s: write i2c error, ret=%d\n", __func__, ret);
				return count;
			}

			ret = i2c_smbus_write_word_data(apds993x_i2c_client,
					CMD_WORD|APDS993X_PIHTL_REG,
					p_data->piht);
			if (ret)
			{
				pr_err("%s: write i2c error, ret=%d\n", __func__, ret);
				return count;
			}
		}
		else
		{
			ret = i2c_smbus_write_word_data(apds993x_i2c_client,
					CMD_WORD|APDS993X_PILTL_REG,
					p_data->pilt);
			if (ret)
			{
				pr_err("%s: write i2c error, ret=%d\n", __func__, ret);
				return count;
			}
			ret = i2c_smbus_write_word_data(apds993x_i2c_client,
					CMD_WORD|APDS993X_PIHTL_REG, 1023);
			if (ret)
			{
				pr_err("%s: write i2c error, ret=%d\n", __func__, ret);
				return count;
			}        
		}
	}
	else
	{
		printk("%s the buf format is error.\n", __func__);
	}
	return count;
}
static struct kobj_attribute ps_thd = 
{
	.attr = {"ps_thd", 0220},
	.store = apds993x_alsps_ps_thd_store,
};

static ssize_t apds993x_alsps_ps_min_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 data;
	if (sscanf(buf, "%d", (unsigned int *)&data) == 1)
	{
		printk(KERN_ERR"%s the buf is %s\n", __func__, buf);
		ps_min = data;
	}
	else
	{
		printk("%s the buf format is error.\n", __func__);
	}
	return count;
}
static ssize_t apds993x_alsps_ps_min_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", ps_min);
}
static struct kobj_attribute ps_min_val = 
{
	.attr = {"ps_min", 0664},
	.show = apds993x_alsps_ps_min_show,
	.store = apds993x_alsps_ps_min_store,
};

static ssize_t apds993x_alsps_algo_info_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 data[5];
	if (sscanf(buf, "%d %d %d %d %d", (unsigned int *)&data[0],(unsigned int *)&data[1], (unsigned int *)&data[2],
				(unsigned int *)&data[3], (unsigned int *)&data[4]) == 5)
	{
		printk(KERN_ERR"%s the buf is %s\n", __func__, buf);
		ps_thd_low_highlight = data[0];
		ps_thd_high_highlight = data[1];
		ps_adjust_max = data[2];
		dirty_adjust_low_thd = data[3];
		dirty_adjust_high_thd = data[4];

	}
	else
	{
		printk("%s the buf format is error.\n", __func__);
	}
	return count;
}
static struct kobj_attribute algo_info = 
{
	.attr = {"algo_info", 0220},
	.store = apds993x_alsps_algo_info_store,
};
static ssize_t apds993x_algo_wakeup_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct apds993x_data *data = i2c_get_clientdata(apds993x_i2c_client);
	wait_event_interruptible(enable_wq, data->enable_ps_sensor);
	printk(KERN_ERR"wait ps enable done\n");
	return snprintf(buf, PAGE_SIZE, "%d\n", data->enable_ps_sensor);
}
static struct kobj_attribute algo_wakeup = 
{
	.attr = {"algo_wakeup", 0444},
	.show = apds993x_algo_wakeup_show,
};
static ssize_t apds993x_far_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct apds993x_data *data = i2c_get_clientdata(apds993x_i2c_client);
	return snprintf(buf, PAGE_SIZE, "%d\n",  data->ps_detection ? 0:1);
}
static struct kobj_attribute far_status = 
{
	.attr = {"far_status", 0444},
	.show = apds993x_far_status_show,
};
#endif

static const struct attribute *apds993x_ftm_attrs[] = 
{
	&enable.attr,
	&prox_raw.attr,
	&als_raw.attr,
#ifdef APDS993X_ALSPS_DYNAMIC_THRESHOLD	
	&name.attr,
	&is_high_light.attr,
	&ps_enable.attr,
	&ps_thd.attr,
	&far_status.attr,	
	&ps_min_val.attr,
	&algo_wakeup.attr,
	&algo_info.attr,
#endif
	NULL
};
static struct dev_ftm apds993x_ftm;
/*--------------------------------------------------------------------------*/
/*
 * I2C init/probing/exit functions
 */
static struct i2c_driver apds993x_driver;
static int apds993x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct apds993x_data *data;
	struct apds993x_platform_data *pdata;
	int err = 0;

	pr_err("%s \n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct apds993x_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
		err = sensor_parse_dt(&client->dev, pdata);
		if (err) {
			pr_err("%s: sensor_parse_dt() err\n", __func__);
			return err;
		}
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			dev_err(&client->dev, "No platform data\n");
			return -ENODEV;
		}
	}

	/* Set the default parameters */
	apds993x_ps_detection_threshold = pdata->prox_threshold;
	apds993x_ps_hsyteresis_threshold = pdata->prox_hsyteresis_threshold;
	apds993x_ps_pulse_number = pdata->prox_pulse;
	apds993x_ps_pgain = pdata->prox_gain;

	apds993x_coe_b = pdata->als_B;
	apds993x_coe_c = pdata->als_C;
	apds993x_coe_d = pdata->als_D;
	apds993x_ga = pdata->ga_value;

	data = kzalloc(sizeof(struct apds993x_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		err = -ENOMEM;
		goto exit;
	}
	pdev_data = data;

	data->platform_data = pdata;
	data->client = client;
	apds993x_i2c_client = client;

	wake_lock_init(&data->ps_wakelock,WAKE_LOCK_SUSPEND, "apds_input_wakelock");

	/* initialize pinctrl */
	err = apds993x_pinctrl_init(data);
	if (err) {
		dev_err(&client->dev, "Can't initialize pinctrl\n");
		goto exit_kfree;
	}
	err = pinctrl_select_state(data->pinctrl, data->pin_default);
	if (err) {
		dev_err(&client->dev,"Can't select pinctrl default state\n");
		goto exit_kfree;
	}

	/* h/w initialization */
	if (pdata->init)
	{   
		err = pdata->init();
		if(err)
		{
			dev_err(&client->dev,"pdata->init failed.\n");
			goto exit_kfree;
		}
	}

	if (pdata->power_on)
	{   
		err = pdata->power_on(true);
		if(err)
		{
			dev_err(&client->dev,"pdata->power_on failed.\n");
			goto exit_pdata_power_on;
		}        
	}

	i2c_set_clientdata(client, data);

	data->enable = 0;	/* default mode is standard */
	data->ps_threshold = apds993x_ps_detection_threshold;
	data->ps_hysteresis_threshold = apds993x_ps_hsyteresis_threshold;
	data->ps_detection = 0;	/* default to no detection */
	data->enable_als_sensor = 0;	// default to 0
	data->enable_ps_sensor = 0;	// default to 0
	data->als_poll_delay = 50;	// default to 100ms
	data->als_atime_index = APDS993X_ALS_RES_19456;	// 100ms ATIME
	data->als_again_index = APDS993X_ALS_GAIN_16X;	// 8x AGAIN
	data->als_reduce = 0;	// no ALS 6x reduction
	data->als_prev_lux = 0;

	err = apds993x_check_chip_id(client);
	if (err) {
		dev_err(&client->dev, "Not a valid chip ID\n");
		err = -ENODEV;
		goto exit_uninit;
	}

	/* calibration */
	if (apds993x_cross_talk_val > 0 && apds993x_cross_talk_val < 1000) {
		data->cross_talk = apds993x_cross_talk_val;
	} else {
		/*
		 * default value: Get the cross-talk value from the memory.
		 * This value is saved during the cross-talk calibration
		 */
		data->cross_talk = DEFAULT_CROSS_TALK;
	}

	mutex_init(&data->update_lock);
	mutex_init(&data->op_mutex);

	INIT_DELAYED_WORK(&data->dwork, apds993x_work_handler);
	INIT_DELAYED_WORK(&data->als_dwork, apds993x_als_polling_work_handler);

	/* Initialize the APDS993X chip */
	err = apds993x_init_device(client);
	if (err) {
		pr_err("%s: Failed to init apds993x\n", __func__);
		goto exit_uninit;
	}

	if (data->irq) {
		err = request_irq(data->irq, apds993x_interrupt,IRQF_TRIGGER_FALLING,APDS993X_DRV_NAME, (void *)client);
		if (err < 0) {
			dev_err(&client->dev,
					"Could not allocate APDS993X_INT !\n");
			goto exit_uninit;
		}
		disable_irq(data->irq);
	}

	/* Register to Input Device */
	data->input_dev_als = input_allocate_device();
	if (!data->input_dev_als) {
		err = -ENOMEM;
		pr_err("%s: Failed to allocate input device als\n", __func__);
		goto exit_free_irq;
	}

	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		err = -ENOMEM;
		pr_err("%s: Failed to allocate input device ps\n", __func__);
		goto exit_free_dev_als;
	}

	set_bit(EV_ABS, data->input_dev_als->evbit);
	set_bit(EV_ABS, data->input_dev_ps->evbit);

	input_set_abs_params(data->input_dev_als, ABS_MISC, 0, 60000, 0, 0);
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 1, 0, 0);

	data->input_dev_als->name = "light";
	data->input_dev_ps->name = "proximity";

	err = input_register_device(data->input_dev_als);
	if (err) {
		err = -ENOMEM;
		pr_err("%s: Unable to register input device als: %s\n",__func__, data->input_dev_als->name);
		goto exit_free_dev_ps;
	}

	err = input_register_device(data->input_dev_ps);
	if (err) {
		err = -ENOMEM;
		pr_err("%s: Unable to register input device ps: %s\n",__func__, data->input_dev_ps->name);
		goto exit_unregister_dev_als;
	}

	/* Register sysfs hooks */
	err = sysfs_create_group(&client->dev.kobj, &apds993x_attr_group);
	if (err)
		goto exit_unregister_dev_ps;

	/* Register to sensors class */
	data->als_cdev = sensors_light_cdev;
	data->als_cdev.sensors_enable = apds993x_als_set_enable;
	data->als_cdev.sensors_poll_delay = apds993x_als_poll_delay;
	data->ps_cdev = sensors_proximity_cdev;
	data->ps_cdev.sensors_enable = apds993x_ps_set_enable;
	data->ps_cdev.sensors_poll_delay = NULL;

	err = sensors_classdev_register(&data->input_dev_als->dev,&data->als_cdev);
	if (err) {
		pr_err("%s: Unable to register to sensors class: %d\n",__func__, err);
		goto exit_unregister_als_ioctl;
	}

	err = sensors_classdev_register(&data->input_dev_ps->dev,&data->ps_cdev);
	if (err) {
		pr_err("%s: Unable to register to sensors class: %d\n", __func__, err);
		goto exit_unregister_als_class;
	}

	if (pdata->power_on)
		err = pdata->power_on(false);

	pr_err("%s: Support ver. %s enabled\n", __func__, DRIVER_VERSION);

#ifdef APDS993X_ALSPS_DYNAMIC_THRESHOLD  

	INIT_DELAYED_WORK(&sample_ps_work, sample_work_func);
	queue_delayed_work(apds993x_workqueue,&sample_ps_work, msecs_to_jiffies(5000)); 

	init_waitqueue_head(&enable_wq);
#endif
	apds993x_ftm.name = "als_prox";
	apds993x_ftm.i2c_client = data->client;
	apds993x_ftm.attrs = apds993x_ftm_attrs;
	apds993x_ftm.priv_data = data;
	register_single_dev_ftm(&apds993x_ftm);

	return 0;

exit_unregister_als_class:
	sensors_classdev_unregister(&data->als_cdev);
exit_unregister_als_ioctl:
	sysfs_remove_group(&client->dev.kobj, &apds993x_attr_group);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_unregister_dev_als:
	input_unregister_device(data->input_dev_als);
exit_free_dev_ps:
exit_free_dev_als:
exit_free_irq:
	free_irq(data->irq, client);
exit_uninit:
	if (pdata->power_on)
		pdata->power_on(false);
exit_pdata_power_on:
	if (pdata->exit)
		pdata->exit();
exit_kfree:
	wake_lock_destroy(&data->ps_wakelock);
	kfree(data);
	pdev_data = NULL;
exit:
	return err;
}

static int apds993x_remove(struct i2c_client *client)
{
	struct apds993x_data *data = i2c_get_clientdata(client);
	struct apds993x_platform_data *pdata = data->platform_data;

	/* Power down the device */
	apds993x_set_enable(client, 0);

	sysfs_remove_group(&client->dev.kobj, &apds993x_attr_group);

	input_unregister_device(data->input_dev_ps);
	input_unregister_device(data->input_dev_als);

	free_irq(client->irq, data);

	wake_lock_destroy(&data->ps_wakelock);

	if (pdata->power_on)
		pdata->power_on(false);

	if (pdata->exit)
		pdata->exit();

	kfree(data);
	pdev_data = NULL;

	return 0;
}

static const struct i2c_device_id apds993x_id[] = {
	{ "apds993x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, apds993x_id);

static struct of_device_id apds993X_match_table[] = {
	{ .compatible = "avago,apds9930",},
	{ .compatible = "avago,apds9900",},
	{ },
};

static const struct dev_pm_ops apds993x_pm_ops = {
	.suspend	= apds993x_suspend,
	.resume 	= apds993x_resume,
};

static struct i2c_driver apds993x_driver = {
	.driver = {
		.name   = APDS993X_DRV_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = apds993X_match_table,
		.pm = &apds993x_pm_ops,
	},
	.probe  = apds993x_probe,
	.remove = apds993x_remove,
	.id_table = apds993x_id,
};

static int __init apds993x_init(void)
{
	apds993x_workqueue = create_workqueue("proximity_als");
	if (!apds993x_workqueue) {
		pr_err("%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	return i2c_add_driver(&apds993x_driver);
}

static void __exit apds993x_exit(void)
{
	if (apds993x_workqueue)
		destroy_workqueue(apds993x_workqueue);
	i2c_del_driver(&apds993x_driver);
}

MODULE_AUTHOR("Lee Kai Koon <kai-koon.lee@avagotech.com>");
MODULE_DESCRIPTION("APDS993X ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(apds993x_init);
module_exit(apds993x_exit);
