/*
*  stmvl53l0_module.c - Linux kernel modules for STM VL53L0 FlightSense TOF
*						 sensor
*
*  Copyright (C) 2015 STMicroelectronics Imaging Division.
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program;
*/
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
/*
* API includes
*/
#include "vl53l0_api.h"
#include "vl53l010_api.h"
/*
#include "vl53l0_def.h"
#include "vl53l0_platform.h"
#include "stmvl53l0-i2c.h"
#include "stmvl53l0-cci.h"
#include "stmvl53l0.h"
*/

#define USE_INT
#define IRQ_NUM	   59
/*#define DEBUG_TIME_LOG*/
#ifdef DEBUG_TIME_LOG
struct timeval start_tv, stop_tv;
#endif

#define SAR_LOW    30
#define SAR_HIGH   40
static char *devname = "laser";

static struct stmvl53l0_data *gp_vl53l0_data;

static struct stmvl53l0_module_fn_t stmvl53l0_module_func_tbl_cci = {
	.init = stmvl53l0_init_cci,
	.deinit = stmvl53l0_exit_cci,
	.power_up = stmvl53l0_power_up_cci,
	.power_down = stmvl53l0_power_down_cci,
};

static struct stmvl53l0_module_fn_t stmvl53l0_module_func_tbl_i2c = {
	.init = stmvl53l0_init_i2c,
	.deinit = stmvl53l0_exit_i2c,
	.power_up = stmvl53l0_power_up_i2c,
	.power_down = stmvl53l0_power_down_i2c,
};

struct stmvl53l0_module_fn_t *pmodule_func_tbl;


struct stmvl53l0_api_fn_t {
	VL53L0_Error(*DataInit)(VL53L0_DEV Dev);
	VL53L0_Error(*GetDeviceInfo)(VL53L0_DEV Dev,
		VL53L0_DeviceInfo_t *pVL53L0_DeviceInfo);
	VL53L0_Error(*StaticInit)(VL53L0_DEV Dev);
	VL53L0_Error(*SetDeviceMode)(VL53L0_DEV Dev,
		VL53L0_DeviceModes DeviceMode);
	VL53L0_Error(*SetLimitCheckValue)(VL53L0_DEV Dev,
		uint16_t LimitCheckId,
		FixPoint1616_t LimitCheckValue);
	VL53L0_Error(*SetLimitCheckEnable)(VL53L0_DEV Dev,
		uint16_t LimitCheckId,
		uint8_t LimitCheckEnable);
	VL53L0_Error(*SetWrapAroundCheckEnable)(VL53L0_DEV Dev,
		uint8_t WrapAroundCheckEnable);
	VL53L0_Error(*PerformXTalkCalibration)(VL53L0_DEV Dev,
		FixPoint1616_t XTalkCalDistance,
		FixPoint1616_t *pXTalkCompensationRateMegaCps);
	VL53L0_Error(*PerformOffsetCalibration)(VL53L0_DEV Dev,
		FixPoint1616_t CalDistanceMilliMeter,
		int32_t *pOffsetMicroMeter);
	VL53L0_Error(*SetGpioConfig)(VL53L0_DEV Dev,
		uint8_t Pin,
		VL53L0_DeviceModes DeviceMode,
		VL53L0_GpioFunctionality Functionality,
		VL53L0_InterruptPolarity Polarity);
	VL53L0_Error(*SetInterruptThresholds)(VL53L0_DEV Dev,
		VL53L0_DeviceModes DeviceMode,
		FixPoint1616_t ThresholdLow,
		FixPoint1616_t ThresholdHigh);
	VL53L0_Error(*ClearInterruptMask)(VL53L0_DEV Dev,
		uint32_t InterruptMask);
	VL53L0_Error(*GetRangingMeasurementData)(VL53L0_DEV Dev,
		VL53L0_RangingMeasurementData_t *pRangingMeasurementData);
	VL53L0_Error(*StartMeasurement)(VL53L0_DEV Dev);
	VL53L0_Error(*SetInterMeasurementPeriodMilliSeconds)(VL53L0_DEV Dev,
		uint32_t InterMeasurementPeriodMilliSeconds);
	VL53L0_Error(*PerformSingleRangingMeasurement)(VL53L0_DEV Dev,
		VL53L0_RangingMeasurementData_t *pRangingMeasurementData);
	VL53L0_Error(*SetXTalkCompensationEnable)(VL53L0_DEV Dev,
		uint8_t XTalkCompensationEnable);
	VL53L0_Error(*StopMeasurement)(VL53L0_DEV Dev);
};

static struct stmvl53l0_api_fn_t stmvl53l0_api_func_tbl = {
	.DataInit = VL53L0_DataInit,
	.GetDeviceInfo = VL53L0_GetDeviceInfo,
	.StaticInit = VL53L0_StaticInit,
	.SetDeviceMode = VL53L0_SetDeviceMode,
	.SetLimitCheckValue = VL53L0_SetLimitCheckValue,
	.SetLimitCheckEnable = VL53L0_SetLimitCheckEnable,
	.SetWrapAroundCheckEnable = VL53L0_SetWrapAroundCheckEnable,
	.PerformXTalkCalibration = VL53L0_PerformXTalkCalibration,
	.PerformOffsetCalibration = VL53L0_PerformOffsetCalibration,
	.SetGpioConfig = VL53L0_SetGpioConfig,
	.SetInterruptThresholds = VL53L0_SetInterruptThresholds,
	.ClearInterruptMask = VL53L0_ClearInterruptMask,
	.GetRangingMeasurementData = VL53L0_GetRangingMeasurementData,
	.StartMeasurement = VL53L0_StartMeasurement,
	.SetInterMeasurementPeriodMilliSeconds =
	VL53L0_SetInterMeasurementPeriodMilliSeconds,
	.PerformSingleRangingMeasurement =
	VL53L0_PerformSingleRangingMeasurement,
};
struct stmvl53l0_api_fn_t *papi_func_tbl;

static void stmvl53l0_setupAPIFunctions(struct stmvl53l0_data *data)
{
	uint8_t revision = 0;
	VL53L0_DEV vl53l0_dev = data;

	/* Read Revision ID */
	VL53L0_RdByte(vl53l0_dev,
		VL53L0_REG_IDENTIFICATION_REVISION_ID, &revision);
	vl53l0_errmsg("read REVISION_ID: 0x%x\n", revision);
	revision = (revision & 0xF0) >> 4;
	if (revision == 1) {
		/*cut 1.1*/
		vl53l0_errmsg("to setup API cut 1.1\n");
		papi_func_tbl->DataInit = VL53L0_DataInit;
		papi_func_tbl->GetDeviceInfo = VL53L0_GetDeviceInfo;
		papi_func_tbl->StaticInit = VL53L0_StaticInit;
		papi_func_tbl->SetDeviceMode = VL53L0_SetDeviceMode;
		papi_func_tbl->SetLimitCheckValue = VL53L0_SetLimitCheckValue;
		papi_func_tbl->SetLimitCheckEnable =
			VL53L0_SetLimitCheckEnable;
		papi_func_tbl->SetWrapAroundCheckEnable =
			VL53L0_SetWrapAroundCheckEnable;
		papi_func_tbl->PerformXTalkCalibration =
			VL53L0_PerformXTalkCalibration;
		papi_func_tbl->PerformOffsetCalibration =
			VL53L0_PerformOffsetCalibration;
		papi_func_tbl->SetGpioConfig = VL53L0_SetGpioConfig;
		papi_func_tbl->SetInterruptThresholds =
			VL53L0_SetInterruptThresholds;
		papi_func_tbl->ClearInterruptMask =
			VL53L0_ClearInterruptMask;
		papi_func_tbl->GetRangingMeasurementData =
			VL53L0_GetRangingMeasurementData;
		papi_func_tbl->StartMeasurement = VL53L0_StartMeasurement;
		papi_func_tbl->SetInterMeasurementPeriodMilliSeconds =
			VL53L0_SetInterMeasurementPeriodMilliSeconds;
		papi_func_tbl->PerformSingleRangingMeasurement =
			VL53L0_PerformSingleRangingMeasurement;
		papi_func_tbl->SetXTalkCompensationEnable =
			VL53L0_SetXTalkCompensationEnable;
		papi_func_tbl->StopMeasurement = VL53L0_StopMeasurement;

	} else if (revision == 0) {
		/*cut 1.0*/
		vl53l0_errmsg("to setup API cut 1.0\n");
		papi_func_tbl->DataInit = VL53L010_DataInit;
		papi_func_tbl->GetDeviceInfo = VL53L010_GetDeviceInfo;
		papi_func_tbl->StaticInit = VL53L010_StaticInit;
		papi_func_tbl->SetDeviceMode = VL53L010_SetDeviceMode;
		papi_func_tbl->SetLimitCheckValue =
			VL53L010_SetLimitCheckValue;
		papi_func_tbl->SetLimitCheckEnable =
			VL53L010_SetLimitCheckEnable;
		papi_func_tbl->SetWrapAroundCheckEnable =
			VL53L010_SetWrapAroundCheckEnable;
		papi_func_tbl->PerformXTalkCalibration =
			VL53L010_PerformXTalkCalibration;
		papi_func_tbl->PerformOffsetCalibration =
			VL53L010_PerformOffsetCalibration;
		papi_func_tbl->SetGpioConfig = VL53L010_SetGpioConfig;
		papi_func_tbl->SetInterruptThresholds =
			VL53L010_SetInterruptThresholds;
		papi_func_tbl->ClearInterruptMask = VL53L010_ClearInterruptMask;
		papi_func_tbl->GetRangingMeasurementData =
			VL53L010_GetRangingMeasurementData;
		papi_func_tbl->StartMeasurement = VL53L010_StartMeasurement;
		papi_func_tbl->SetInterMeasurementPeriodMilliSeconds =
			VL53L010_SetInterMeasurementPeriodMilliSeconds;
		papi_func_tbl->PerformSingleRangingMeasurement =
			VL53L010_PerformSingleRangingMeasurement;
		papi_func_tbl->SetXTalkCompensationEnable =
			VL53L010_SetXTalkCompensationEnable;
		papi_func_tbl->StopMeasurement = VL53L010_StopMeasurement;
	}

}
/*
 * IOCTL definitions
 */
#define VL53L0_IOCTL_INIT			_IO('p', 0x01)
#define VL53L0_IOCTL_XTALKCALB		_IO('p', 0x02)
#define VL53L0_IOCTL_OFFCALB		_IO('p', 0x03)
#define VL53L0_IOCTL_STOPCALB		_IO('p', 0x04)
#define VL53L0_IOCTL_STOP			_IO('p', 0x05)
#define VL53L0_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
#define VL53L0_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL53L0_IOCTL_GETDATAS \
			_IOR('p', 0x0b, VL53L0_RangingMeasurementData_t)
#define VL53L0_IOCTL_REGISTER \
			_IOWR('p', 0x0c, struct stmvl53l0_register)
#define VL53L0_IOCTL_PARAMETER \
			_IOWR('p', 0x0d, struct stmvl53l0_parameter)


static long stmvl53l0_ioctl(struct file *file,
							unsigned int cmd,
							unsigned long arg);
static int stmvl53l0_flush(struct file *file, fl_owner_t id);
static int stmvl53l0_open(struct inode *inode, struct file *file);
static int stmvl53l0_close(struct inode *inode, struct file *file);
static int stmvl53l0_init_client(struct stmvl53l0_data *data);
static int stmvl53l0_start(struct stmvl53l0_data *data);
static int stmvl53l0_stop(struct stmvl53l0_data *data);

void stmvl53l0_dumpreg(struct stmvl53l0_data *data)
{
	int i = 0;
	uint8_t reg;

	if (data->enableDebug) {
		for (i = 1; i <= 0xff; i++) {
			VL53L0_RdByte(data, i, &reg);
			pr_err("STM VL53L0 reg= %x. value:%x.\n", i, reg);
		}
	}

}


static void stmvl53l0_ps_read_measurement(struct stmvl53l0_data *data)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	data->ps_data = data->rangeData.RangeMilliMeter;
	input_report_abs(data->input_dev_ps, ABS_DISTANCE,
		(int)(data->ps_data + 5) / 10);
	input_report_abs(data->input_dev_ps, ABS_HAT0X, tv.tv_sec);
	input_report_abs(data->input_dev_ps, ABS_HAT0Y, tv.tv_usec);
	input_report_abs(data->input_dev_ps, ABS_HAT1X,
		data->rangeData.RangeMilliMeter);
	input_report_abs(data->input_dev_ps, ABS_HAT1Y,
		data->rangeData.RangeStatus);
	input_report_abs(data->input_dev_ps, ABS_HAT2X,
		data->rangeData.SignalRateRtnMegaCps);
	input_report_abs(data->input_dev_ps, ABS_HAT2Y,
		data->rangeData.AmbientRateRtnMegaCps);
	input_report_abs(data->input_dev_ps, ABS_HAT3X,
		data->rangeData.MeasurementTimeUsec);
	input_report_abs(data->input_dev_ps, ABS_HAT3Y,
		data->rangeData.RangeDMaxMilliMeter);
	input_sync(data->input_dev_ps);

	if (data->enableDebug)
		vl53l0_errmsg("range:%d, signalRateRtnMegaCps:%d, \
				error:0x%x,rtnambrate:%u,measuretime:%u\n",
				data->rangeData.RangeMilliMeter,
				data->rangeData.SignalRateRtnMegaCps,
				data->rangeData.RangeStatus,
				data->rangeData.AmbientRateRtnMegaCps,
				data->rangeData.MeasurementTimeUsec);

}

static void stmvl53l0_enter_off(struct stmvl53l0_data *data, uint8_t from)
{
	vl53l0_dbgmsg("Enter, stmvl53l0_enter_off from:%d\n", from);
	mutex_lock(&data->work_mutex);
		/* turn off tof sensor */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0_stop(data);
		}

	vl53l0_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);
}
static void stmvl53l0_enter_cam(struct stmvl53l0_data *data, uint8_t from)
{
	mutex_lock(&data->work_mutex);
	vl53l0_dbgmsg("Enter, stmvl53l0_enter_cam from:%d\n", from);
	/* turn on tof sensor */
	if (data->enable_ps_sensor == 0)
		stmvl53l0_start(data);

	vl53l0_dbgmsg("Call of VL53L0_DEVICEMODE_CONTINUOUS_RANGING\n");
	papi_func_tbl->SetGpioConfig(data, 0, 0,
		VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY,
		VL53L0_INTERRUPTPOLARITY_LOW);
	papi_func_tbl->SetDeviceMode(data,
		VL53L0_DEVICEMODE_CONTINUOUS_RANGING);
	papi_func_tbl->StartMeasurement(data);

	vl53l0_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);
}
static void stmvl53l0_enter_sar(struct stmvl53l0_data *data, uint8_t from)
{
	mutex_lock(&data->work_mutex);
	vl53l0_dbgmsg("Enter,  from:%d\n", from);
	/* turn on tof sensor */
	if (data->enable_ps_sensor == 0)
		stmvl53l0_start(data);

	papi_func_tbl->SetInterMeasurementPeriodMilliSeconds
		(data, data->delay_ms);

	papi_func_tbl->SetGpioConfig(data, 0, 0,
		VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW,
		VL53L0_INTERRUPTPOLARITY_LOW);
	papi_func_tbl->SetInterruptThresholds(data,
		0, data->lowv << 16, data->highv << 16);
	papi_func_tbl->SetDeviceMode(data,
		VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING);
	data->lowint = 1;
	msleep(20);
	papi_func_tbl->StartMeasurement(data);
	vl53l0_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);
	stmvl53l0_dumpreg(data);
}
static void stmvl53l0_enter_super(struct stmvl53l0_data *data, uint8_t from)
{
	mutex_lock(&data->work_mutex);
	vl53l0_dbgmsg("Enter, stmvl53l0_enter_super flag:%d\n", from);
	if (from == SAR_MODE) {
		vl53l0_dbgmsg("Call of VL53L0_DEVICEMODE_CONTINUOUS_RANGING\n");
		papi_func_tbl->SetGpioConfig(data, 0, 0,
			VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY,
			VL53L0_INTERRUPTPOLARITY_LOW);
		papi_func_tbl->SetDeviceMode(data,
			VL53L0_DEVICEMODE_CONTINUOUS_RANGING);
		papi_func_tbl->StartMeasurement(data);
	}

	vl53l0_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);
}
static void stmvl53l0_enter_xtalkcal(struct stmvl53l0_data *data,
	uint8_t from)
{
	FixPoint1616_t XTalkCompensationRateMegaCps;

	mutex_lock(&data->work_mutex);
	vl53l0_dbgmsg("Enter, stmvl53l0_enter_cal flag:%d\n", from);
	if (data->enable_ps_sensor == 0)
		stmvl53l0_start(data);

	VL53L0_SetXTalkCompensationEnable(data, 0);

	VL53L0_PerformXTalkCalibration(data,
		(100 << 16), &XTalkCompensationRateMegaCps);
	vl53l0_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);
}
static void stmvl53l0_enter_offsetcal(struct stmvl53l0_data *data,
	uint8_t from)
{
	mutex_lock(&data->work_mutex);
	vl53l0_dbgmsg("Enter, stmvl53l0_enter_cal flag:%d\n", from);
	if (data->enable_ps_sensor == 0)
		stmvl53l0_start(data);

	VL53L0_SetXTalkCompensationEnable(data, 0);
	VL53L0_SetOffsetCalibrationDataMicroMeter(data, 0);
	vl53l0_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);
}
static void stmvl53l0_work_state(
struct stmvl53l0_data *data, uint8_t input)
{
	unsigned long flags;
	int nochange = 0;
	uint8_t from = data->w_mode;

	vl53l0_dbgmsg("Enter, stmvl53l0_work_state:%d\n", input);
	spin_lock_irqsave(&data->update_lock.wait_lock, flags);
	switch (data->w_mode) {
	case OFF_MODE:
		if (input == CAM_ON)
			data->w_mode = CAM_MODE;
		else if (input == SAR_ON)
			data->w_mode = SAR_MODE;
		else if (input == XTALKCAL_ON)
			data->w_mode = XTALKCAL_MODE;
		else if (input == OFFSETCAL_ON)
			data->w_mode = OFFSETCAL_MODE;
		else {
			vl53l0_dbgmsg("unsopport status = %d,cs = OFF_MODE",
				input);
			nochange = 1;
		}
		break;
	case CAM_MODE:
		if (input == CAM_OFF)
			data->w_mode = OFF_MODE;
		else if (input == SAR_ON)
			data->w_mode = SUPER_MODE;
		else {
		vl53l0_dbgmsg("unsopport status = %d,cs = CAM_MODE", input);
			nochange = 1;
		}
		break;
	case SAR_MODE:
		if (input == CAM_ON)
			data->w_mode = SUPER_MODE;
		else if (input == SAR_OFF)
			data->w_mode = OFF_MODE;
		else {
		vl53l0_dbgmsg("unsopport status = %d,cs = SAR_MODE", input);
			nochange = 1;
		}
		break;
	case SUPER_MODE:
		if (input == CAM_OFF)
			data->w_mode = SAR_MODE;
		else if (input == SAR_OFF)
			data->w_mode = CAM_MODE;
		else {
		vl53l0_dbgmsg("unsopport status= %d,cs = SUPER_MODE", input);
			nochange = 1;
		}
		break;
	case XTALKCAL_MODE:
		if (input == CAL_OFF)
			data->w_mode = OFF_MODE;
		else {
		vl53l0_dbgmsg("unsopport status = %d,cs= SUPER_MODE", input);
			data->w_mode = OFF_MODE;
		}
		break;
	case OFFSETCAL_MODE:
		if (input == CAL_OFF)
			data->w_mode = OFF_MODE;
		else {
		vl53l0_dbgmsg("unsopport status= %d,cs = SUPER_MODE", input);
			data->w_mode = OFF_MODE;
		}
		break;
	default:
		vl53l0_dbgmsg("unsopport status= %d,cs = unknown", input);
		nochange = 1;
		break;
	}

	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);
	if (nochange) {
		vl53l0_dbgmsg("unsopport status= %d,cs = %d",
		input, data->w_mode);
		return;
	}
	switch (data->w_mode) {
	case OFF_MODE:
		stmvl53l0_enter_off(data, from);
		break;
	case CAM_MODE:
		stmvl53l0_enter_cam(data, from);
		break;
	case SAR_MODE:
		stmvl53l0_enter_sar(data, from);
		break;
	case SUPER_MODE:
		stmvl53l0_enter_super(data, from);
		break;
	case XTALKCAL_MODE:
		stmvl53l0_enter_xtalkcal(data, from);
		break;
	case OFFSETCAL_MODE:
		stmvl53l0_enter_offsetcal(data, from);
		break;
	default:
		pr_err("status unknown, input = %d", input);
		break;
	}
}

static void stmvl53l0_cancel_handler(struct stmvl53l0_data *data)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&data->update_lock.wait_lock, flags);
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	ret = cancel_delayed_work(&data->dwork);
	if (ret == 0)
		vl53l0_errmsg("%d,cancel_delayed_work return FALSE\n",
		__LINE__);

	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

	return;
}

static void stmvl53l0_state_process(void)
{
VL53L0_RangingMeasurementData_t    RMData;
struct stmvl53l0_data *data = gp_vl53l0_data;
VL53L0_DEV vl53l0_dev = data;
char *envplow[2] = { "SAR LOW=1", NULL };
char *envphigh[2] = { "SAR HIGH=1", NULL };

papi_func_tbl->GetRangingMeasurementData(vl53l0_dev, &RMData);

vl53l0_dbgmsg("which MODE =%d\n", data->w_mode);
VL53L0_ClearInterruptMask(vl53l0_dev, 0);
memcpy(&(data->rangeData), &RMData,
	sizeof(VL53L0_RangingMeasurementData_t));
	if (CAM_MODE == data->w_mode) {
		vl53l0_dbgmsg("CAM_MODE\n");
		if (data->enableDebug)
			pr_err("range:%d, signalRateRtnMegaCps:%d",
			RMData.RangeMilliMeter,
			RMData.SignalRateRtnMegaCps);
	} else if (SAR_MODE == data->w_mode) {
		vl53l0_dbgmsg("SAR_MODE\n");
		if (RMData.RangeMilliMeter < data->lowv) {
			vl53l0_dbgmsg("SAR enter LOW\n");
			stmvl53l0_ps_read_measurement(vl53l0_dev);
			papi_func_tbl->SetGpioConfig(data, 0, 0,
			VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH,
			VL53L0_INTERRUPTPOLARITY_LOW);
			papi_func_tbl->SetInterMeasurementPeriodMilliSeconds(
				data, data->delay_ms);
			VL53L0_SetMeasurementTimingBudgetMicroSeconds(
				data, 120);
			papi_func_tbl->SetInterruptThresholds(
			data, 0, data->lowv << 16, data->highv << 16);
			papi_func_tbl->SetDeviceMode(data,
				VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING);
			data->lowint = 0;
			papi_func_tbl->StartMeasurement(data);
			kobject_uevent_env(&(data->miscdev.this_device->kobj),
				KOBJ_CHANGE, envplow);
			vl53l0_dbgmsg("SAR enter LOW sent uevent\n");
		} else if (RMData.RangeMilliMeter > data->highv) {
			vl53l0_dbgmsg("SAR enter HIGH\n");
			stmvl53l0_ps_read_measurement(vl53l0_dev);
			papi_func_tbl->SetGpioConfig(data, 0, 0,
			VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW,
			VL53L0_INTERRUPTPOLARITY_LOW);
			papi_func_tbl->SetInterMeasurementPeriodMilliSeconds(
				data, data->delay_ms);
			VL53L0_SetMeasurementTimingBudgetMicroSeconds(
				data, 33);
			papi_func_tbl->SetInterruptThresholds(
				data, 0, data->lowv << 16, data->highv << 16);
			papi_func_tbl->SetDeviceMode(data,
				VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING);
			data->lowint = 1;
			papi_func_tbl->StartMeasurement(data);
			kobject_uevent_env(&(data->miscdev.this_device->kobj),
				KOBJ_CHANGE, envphigh);
		}
	} else if (SUPER_MODE == data->w_mode) {
		vl53l0_dbgmsg("SUPER_MODE\n");
		if (RMData.RangeMilliMeter < data->lowv
				&& RMData.RangeMilliMeter != 0) {
			vl53l0_dbgmsg("SAR enter LOW\n");
			stmvl53l0_ps_read_measurement(vl53l0_dev);
			kobject_uevent_env(&(data->miscdev.this_device->kobj),
				KOBJ_CHANGE, envplow);
		} else if (RMData.RangeMilliMeter > data->highv) {
			vl53l0_dbgmsg("SAR enter HIGH\n");
			stmvl53l0_ps_read_measurement(vl53l0_dev);
			kobject_uevent_env(&(data->miscdev.this_device->kobj),
				KOBJ_CHANGE, envphigh);
		}
		papi_func_tbl->StartMeasurement(data);
	}

}
/* interrupt work handler */
static void stmvl53l0_work_handler(struct work_struct *work)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;
	VL53L0_DEV vl53l0_dev = data;
	uint32_t InterruptMask;

	vl53l0_dbgmsg("get in the handler\n");

	mutex_lock(&data->work_mutex);

	if (data->enable_ps_sensor == 1) {
		VL53L0_GetInterruptMaskStatus(vl53l0_dev, &InterruptMask);

		vl53l0_dbgmsg(" InterruptMasksssss :%d\n", InterruptMask);
		if (InterruptMask > 0 && data->interrupt_received == 1) {
			data->interrupt_received = 0;
			stmvl53l0_state_process();
		}
	}

	mutex_unlock(&data->work_mutex);

	return;
}

/*
 * SysFS support
 */
static ssize_t stmvl53l0_show_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;

	return snprintf(buf, 2, "%d\n", data->enable_ps_sensor);
}

static ssize_t stmvl53l0_store_enable_ps_sensor(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;

	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return count;

	if ((val != 0) && (val != 1)) {
		vl53l0_errmsg("%d,store unvalid value=%ld\n", __LINE__, val);
		return count;
	}
	mutex_lock(&data->work_mutex);
	vl53l0_dbgmsg("Enter, enable_ps_sensor flag:%d\n",
		data->enable_ps_sensor);
	vl53l0_dbgmsg("enable ps senosr ( %ld)\n", val);

	if (val == 1) {
		/* turn on tof sensor */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data);
		} else {
			vl53l0_errmsg("%d,Already enabled. Skip !", __LINE__);
		}
	} else {
		/* turn off tof sensor */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0_stop(data);
		}
	}
	vl53l0_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);

	return count;
}

static DEVICE_ATTR(enable_ps_sensor, S_IWUSR | S_IWGRP | S_IRUGO,
	stmvl53l0_show_enable_ps_sensor, stmvl53l0_store_enable_ps_sensor);
static ssize_t stmvl53l0_show_set_mode(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;

	return snprintf(buf, 3, "%d\n", data->d_mode);
}

/* for debug */
static ssize_t stmvl53l0_store_set_mode(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;
	VL53L0_RangingMeasurementData_t    RMData;
	long on;
	VL53L0_Error Status = VL53L0_ERROR_NONE;

	if (kstrtoul(buf, 10, &on))
		return count;
	if (on > 6) {
		vl53l0_errmsg("%d,set d_mode=%ld\n", __LINE__, on);
		return count;
	}
	data->d_mode = on;
	/* turn on tof sensor */
	if (data->enable_ps_sensor == 0) {
		/* to start */
		stmvl53l0_start(data);
	}
	if (VL53L0_DEVICEMODE_SINGLE_RANGING == data->d_mode) {
		vl53l0_dbgmsg("Call of VL53L0_DEVICEMODE_SINGLE_RANGING\n");
		Status = papi_func_tbl->SetGpioConfig(data, 0, 0,
			VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY,
			VL53L0_INTERRUPTPOLARITY_LOW);
		Status = papi_func_tbl->SetDeviceMode(data,
			VL53L0_DEVICEMODE_SINGLE_RANGING);
		VL53L0_SetMeasurementTimingBudgetMicroSeconds(data, 50);
		Status = papi_func_tbl->PerformSingleRangingMeasurement(data,
			&RMData);
		stmvl53l0_ps_read_measurement(data);
	} else if (1 == data->d_mode) {
		vl53l0_dbgmsg("Call of mode 1\n");
		Status = papi_func_tbl->SetGpioConfig(data, 0, 0,
			VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY,
			VL53L0_INTERRUPTPOLARITY_LOW);
		Status = papi_func_tbl->SetDeviceMode(data,
			VL53L0_DEVICEMODE_CONTINUOUS_RANGING);
		papi_func_tbl->StartMeasurement(data);
	} else if (2 == data->d_mode) {
		vl53l0_dbgmsg("Call of mode 2\n");
		Status = papi_func_tbl->SetGpioConfig(data, 0, 0,
			VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW,
			VL53L0_INTERRUPTPOLARITY_LOW);
		Status = papi_func_tbl->SetDeviceMode(data,
			VL53L0_DEVICEMODE_CONTINUOUS_RANGING);
		papi_func_tbl->SetInterruptThresholds
			(data, 0, 60 << 16, 80 << 16);
		papi_func_tbl->StartMeasurement(data);

	} else if (3 == data->d_mode) {
		vl53l0_dbgmsg("Call  mode 3\n");
		papi_func_tbl->SetInterMeasurementPeriodMilliSeconds
			(data, 0x00001388);
		papi_func_tbl->SetGpioConfig(data, 0, 0,
			VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY,
			VL53L0_INTERRUPTPOLARITY_LOW);

		papi_func_tbl->SetInterruptThresholds
			(data, 0, 30 << 16, 40 << 16);
		Status = papi_func_tbl->SetDeviceMode(data,
			VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING);
		papi_func_tbl->StartMeasurement(data);
	} else if (4 == data->d_mode) {
		vl53l0_dbgmsg("Call of mode 4\n");
		Status = papi_func_tbl->SetGpioConfig(data, 0, 0,
			VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH,
			VL53L0_INTERRUPTPOLARITY_LOW);
		Status = papi_func_tbl->SetDeviceMode(data,
			VL53L0_DEVICEMODE_CONTINUOUS_RANGING);
		papi_func_tbl->SetInterruptThresholds
			(data, 0, 30 << 16, 40 << 16);
		papi_func_tbl->StartMeasurement(data);
	} else if (5 == data->d_mode)
		stmvl53l0_work_state(data, CAM_ON);
	else if (6 == data->d_mode)
		stmvl53l0_work_state(data, CAM_OFF);

	return count;
}
/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_mode, S_IWUSR | S_IRUGO,
	stmvl53l0_show_set_mode, stmvl53l0_store_set_mode);

static ssize_t stmvl53l0_show_enable_sar(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;

	return snprintf(buf, 3, "%d\n", data->w_mode);
}

/* for debug */
static ssize_t stmvl53l0_store_enable_sar(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;
	long on;

	if (kstrtoul(buf, 10, &on))
		return count;
	if ((on != 0) && (on != 1)) {
		vl53l0_errmsg("%d,set debug=%ld\n", __LINE__, on);
		return count;
	}

	if (on)
		stmvl53l0_work_state(data, SAR_ON);
	else
		stmvl53l0_work_state(data, SAR_OFF);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(enable_sar, S_IWUSR | S_IRUGO,
	stmvl53l0_show_enable_sar, stmvl53l0_store_enable_sar);


static ssize_t stmvl53l0_show_enable_debug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;

	return snprintf(buf, 3, "%d\n", data->enableDebug);
}

/* for debug */
static ssize_t stmvl53l0_store_enable_debug(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;
	long on;

	if (kstrtoul(buf, 10, &on))
		return count;
	if ((on != 0) &&  (on != 1)) {
		vl53l0_errmsg("%d,set debug=%ld\n", __LINE__, on);
		return count;
	}
	data->enableDebug = on;

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(enable_debug, S_IWUSR | S_IRUGO,
	stmvl53l0_show_enable_debug, stmvl53l0_store_enable_debug);

static ssize_t stmvl53l0_show_set_delay_ms(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;

	return snprintf(buf, 3, "%d\n", data->delay_ms);
}

/* for work handler scheduler time */
static ssize_t stmvl53l0_store_set_delay_ms(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;
	long delay_ms;

	if (kstrtoul(buf, 10, &delay_ms))
		return count;
	if (delay_ms == 0) {
		vl53l0_errmsg("%d,set delay_ms=%ld\n", __LINE__, delay_ms);
		return count;
	}
	mutex_lock(&data->work_mutex);
	data->delay_ms = delay_ms;
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_delay_ms, S_IWUSR | S_IWGRP | S_IRUGO,
	stmvl53l0_show_set_delay_ms, stmvl53l0_store_set_delay_ms);

static ssize_t stmvl53l0_show_near(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;

	return snprintf(buf, 5, "%d\n",
		data->rangeData.RangeMilliMeter);
}

/* for work handler scheduler time */
static ssize_t stmvl53l0_store_near(struct device *dev,
struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(near, S_IRUGO,
	stmvl53l0_show_near, stmvl53l0_store_near);


static struct attribute *stmvl53l0_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_debug.attr,
	&dev_attr_set_delay_ms.attr ,
	&dev_attr_set_mode.attr,
	&dev_attr_enable_sar.attr,
	&dev_attr_near.attr,
	NULL
};


static const struct attribute_group stmvl53l0_attr_group = {
	.attrs = stmvl53l0_attributes,
};


/*
* misc device file operation functions
*/
static int stmvl53l0_ioctl_handler(struct file *file,
	unsigned int cmd, unsigned long arg, void __user *p)
{
	int rc = 0;
	unsigned int xtalkint = 0;
	int8_t offsetint = 0;
	struct stmvl53l0_data *data = gp_vl53l0_data;

	struct stmvl53l0_register reg;
	struct stmvl53l0_parameter parameter;
	VL53L0_DEV vl53l0_dev = data;
	uint8_t page_num = 0;
	if (!data)
		return -EINVAL;

	vl53l0_dbgmsg("Enter enable_ps_sensor:%d\n", data->enable_ps_sensor);
	switch (cmd) {
		/* enable */
	case VL53L0_IOCTL_INIT:
		vl53l0_dbgmsg("VL53L0_IOCTL_INIT\n");
		stmvl53l0_work_state(data, CAM_ON);
		break;
		/* crosstalk calibration */
	case VL53L0_IOCTL_XTALKCALB:
		vl53l0_dbgmsg("VL53L0_IOCTL_XTALKCALB\n");
		stmvl53l0_work_state(data, XTALKCAL_ON);
		break;
		/* set up Xtalk value */
	case VL53L0_IOCTL_SETXTALK:
		vl53l0_dbgmsg("VL53L0_IOCTL_SETXTALK\n");
		if (copy_from_user(&xtalkint, (unsigned int *)p,
			sizeof(unsigned int))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl53l0_dbgmsg("SETXTALK as 0x%x\n", xtalkint);
		break;
		/* offset calibration */
	case VL53L0_IOCTL_OFFCALB:
		vl53l0_dbgmsg("VL53L0_IOCTL_OFFCALB\n");
		stmvl53l0_work_state(data, OFFSETCAL_ON);
		break;
	case VL53L0_IOCTL_STOPCALB:
		vl53l0_dbgmsg("VL53L0_IOCTL_OFFCALB\n");
		stmvl53l0_work_state(data, CAL_OFF);
		break;
		/* set up offset value */
	case VL53L0_IOCTL_SETOFFSET:
		vl53l0_dbgmsg("VL53L0_IOCTL_SETOFFSET\n");
		if (copy_from_user(&offsetint, (int8_t *)p, sizeof(int8_t))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl53l0_dbgmsg("SETOFFSET as %d\n", offsetint);

		break;
		/* disable */
	case VL53L0_IOCTL_STOP:
		vl53l0_dbgmsg("VL53L0_IOCTL_STOP\n");
		stmvl53l0_work_state(data, CAL_OFF);
		break;
		/* Get all range data */
	case VL53L0_IOCTL_GETDATAS:
		vl53l0_dbgmsg("VL53L0_IOCTL_GETDATAS\n");
		if (copy_to_user((VL53L0_RangingMeasurementData_t *)p,
			&(data->rangeData),
			sizeof(VL53L0_RangingMeasurementData_t))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
		/* Register tool */
	case VL53L0_IOCTL_REGISTER:
		vl53l0_dbgmsg("VL53L0_IOCTL_REGISTER\n");
		/* turn on tof sensor */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data);
		}
		if (copy_from_user(&reg, (struct stmvl53l0_register *)p,
			sizeof(struct stmvl53l0_register))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		reg.status = 0;
		page_num = (uint8_t)((reg.reg_index & 0x0000ff00) >> 8);
		vl53l0_dbgmsg("IOCTL_REGISTER, page num:%d\n", page_num);
		if (page_num != 0)
			reg.status = VL53L0_WrByte(vl53l0_dev, 0xFF, page_num);

		switch (reg.reg_bytes) {
		case(4):
			if (reg.is_read)
				reg.status = VL53L0_RdDWord(vl53l0_dev,
				(uint8_t)reg.reg_index, &reg.reg_data);
			else
				reg.status = VL53L0_WrDWord(vl53l0_dev,
				(uint8_t)reg.reg_index, reg.reg_data);
			break;
		case(2):
			if (reg.is_read)
				reg.status = VL53L0_RdWord(vl53l0_dev,
				(uint8_t)reg.reg_index,
				(uint16_t *)&reg.reg_data);
			else
				reg.status = VL53L0_WrWord(vl53l0_dev,
				(uint8_t)reg.reg_index,
				(uint16_t)reg.reg_data);
			break;
		case(1):
			if (reg.is_read)
				reg.status = VL53L0_RdByte(vl53l0_dev,
				(uint8_t)reg.reg_index,
				(uint8_t *)&reg.reg_data);
			else
				reg.status = VL53L0_WrByte(vl53l0_dev,
				(uint8_t)reg.reg_index,
				(uint8_t)reg.reg_data);
			break;
		default:
			reg.status = -1;

		}
		if (page_num != 0)
			reg.status = VL53L0_WrByte(vl53l0_dev, 0xFF, 0);


		if (copy_to_user((struct stmvl53l0_register *)p, &reg,
			sizeof(struct stmvl53l0_register))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
		/* parameter access */
	case VL53L0_IOCTL_PARAMETER:
		vl53l0_dbgmsg("VL53L0_IOCTL_PARAMETER\n");
		/* turn on tof sensor */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data);
		}
		if (copy_from_user(&parameter, (struct stmvl53l0_parameter *)p,
			sizeof(struct stmvl53l0_parameter))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		parameter.status = 0;
		switch (parameter.name) {
		case (OFFSET_PAR):
			if (parameter.is_read)
				parameter.status =
				VL53L0_GetOffsetCalibrationDataMicroMeter(
				vl53l0_dev, &parameter.value);
			else
				parameter.status =
				VL53L0_SetOffsetCalibrationDataMicroMeter(
					vl53l0_dev, parameter.value);
			break;
		case (XTALKRATE_PAR):
			if (parameter.is_read)
				parameter.status =
				VL53L0_GetXTalkCompensationRateMegaCps(
				vl53l0_dev, (FixPoint1616_t *)&parameter.value);
			else
				parameter.status =
				VL53L0_SetXTalkCompensationRateMegaCps(
				vl53l0_dev, (FixPoint1616_t)parameter.value);

			break;
		case (XTALKENABLE_PAR):
			if (parameter.is_read)
				parameter.status =
				VL53L0_GetXTalkCompensationEnable(
				vl53l0_dev, (uint8_t *)&parameter.value);
			else
				parameter.status =
				VL53L0_SetXTalkCompensationEnable(
				vl53l0_dev, (uint8_t)parameter.value);
			break;
		case (SIGMAVAL_PRA):
			if (!parameter.is_read)
				parameter.status = papi_func_tbl->
				SetLimitCheckValue(vl53l0_dev,
				VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
				(FixPoint1616_t)parameter.value);
			break;

		case (SIGMACTL_PRA):
			if (!parameter.is_read)
				parameter.status =
				papi_func_tbl->SetLimitCheckEnable(vl53l0_dev,
				VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
				(uint8_t)parameter.value);
			break;
		case (SGLVAL_PRA):
			if (!parameter.is_read)
				parameter.status = papi_func_tbl->
				SetLimitCheckValue(vl53l0_dev,
				VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
				(FixPoint1616_t)parameter.value);
			break;

		case (SGLCTL_PRA):
			if (!parameter.is_read)
				parameter.status =
				papi_func_tbl->SetLimitCheckEnable(vl53l0_dev,
				VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
				(uint8_t)parameter.value);
			break;

		case (WRAPAROUNDCTL_PRA):
			if (parameter.is_read)
				parameter.status =
				VL53L0_GetWrapAroundCheckEnable(
				vl53l0_dev, (uint8_t *)&parameter.value);
			else
				parameter.status =
				VL53L0_SetWrapAroundCheckEnable(
				vl53l0_dev, (uint8_t)parameter.value);
			break;

		case (MEASUREMENTTIMINGBUDGET_PAR):
			if (parameter.is_read)
				parameter.status =
				VL53L0_GetMeasurementTimingBudgetMicroSeconds(
				vl53l0_dev, (uint32_t *)&parameter.value);
			else
				parameter.status =
				VL53L0_SetMeasurementTimingBudgetMicroSeconds(
				vl53l0_dev, (uint32_t)parameter.value);
			break;
		}
		if (copy_to_user((struct stmvl53l0_parameter *)p, &parameter,
			sizeof(struct stmvl53l0_parameter))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;

	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int stmvl53l0_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int stmvl53l0_close(struct inode *inode, struct file *file)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;

	if (data->w_mode == CAM_MODE || data->w_mode == SUPER_MODE)
		stmvl53l0_work_state(data, CAM_OFF);
	if (data->w_mode == XTALKCAL_MODE || data->w_mode == OFFSETCAL_MODE)
		stmvl53l0_work_state(data, CAL_OFF);
	return 0;
}

static int stmvl53l0_flush(struct file *file, fl_owner_t id)
{
	return 0;
}

static long stmvl53l0_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int ret;
	ret = stmvl53l0_ioctl_handler(file, cmd, arg, (void __user *)arg);
	return ret;
}

#ifdef CONFIG_COMPAT
static long stmvl53l0_compat_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg)
{
	int ret;

	ret = stmvl53l0_ioctl_handler(file, cmd, arg, compat_ptr(arg));
	return ret;
}
#endif

int stmvl53l0_checkmoduleid(struct stmvl53l0_data *data,
	void *client, uint8_t type)
{
	uint8_t id = 0;
	int err;

	vl53l0_dbgmsg("Enter\n");
	if (type == CCI_BUS) {
		struct msm_camera_i2c_client *cci_pclient =
			(struct msm_camera_i2c_client *)client;

		cci_pclient->i2c_func_tbl->i2c_read_seq(cci_pclient,
			0xc0, &id, 1);
	} else {
		struct i2c_client *i2c_pclient = (struct i2c_client *)client;
		struct i2c_msg msg[1];
		uint8_t buff;

		buff = 0xc0;
		msg[0].addr = i2c_pclient->addr;
		msg[0].flags = I2C_M_WR;
		msg[0].buf = &buff;
		msg[0].len = 1;

		err = i2c_transfer(i2c_pclient->adapter, msg, 1);
		if (err != 1) {
			pr_err("%s: i2c_transfer err:%d, addr:0x%x, reg:0x%x\n",
				__func__, err, i2c_pclient->addr, buff);
			err = -1;
			return err;
		}

		msg[0].addr = i2c_pclient->addr;
		msg[0].flags = I2C_M_RD | i2c_pclient->flags;
		msg[0].buf = &id;
		msg[0].len = 1;

		err = i2c_transfer(i2c_pclient->adapter, &msg[0], 1);
		if (err != 1) {
			pr_err("%s: Read i2c_transfer err:%d, addr:0x%x\n",
				__func__, err, i2c_pclient->addr);
			err = -1;
			return err;
		}
	}
	/* Read Model ID */

	vl53l0_dbgmsg("read MODLE_ID: 0x%x\n", id);
	if (id == 0xee) {
		vl53l0_errmsg("STM VL53L0 Found %d\n", __LINE__);
		return 0;
	}

	vl53l0_errmsg("Not found STM VL53L0 %d\n", __LINE__);
		return -EIO;


}

/*
* Initialization function
*/
static int stmvl53l0_init_client(struct stmvl53l0_data *data)
{
	uint8_t id = 0, type = 0;
	uint8_t revision = 0, module_id = 0;
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceInfo_t DeviceInfo;
	VL53L0_DEV vl53l0_dev = data;
	FixPoint1616_t	SigmaLimitValue;

	vl53l0_dbgmsg("Enter\n");


	/* Read Model ID */
	VL53L0_RdByte(vl53l0_dev, VL53L0_REG_IDENTIFICATION_MODEL_ID, &id);
	vl53l0_errmsg("read MODLE_ID: 0x%x\n", id);
	if (id == 0xee) {
		vl53l0_errmsg("STM VL53L0 Found\n");
	} else if (id == 0) {
		vl53l0_errmsg("Not found STM VL53L0\n");
		return -EIO;
	}
	VL53L0_RdByte(vl53l0_dev, 0xc1, &id);
	vl53l0_errmsg("read 0xc1: 0x%x\n", id);
	VL53L0_RdByte(vl53l0_dev, 0xc2, &id);
	vl53l0_errmsg("read 0xc2: 0x%x\n", id);
	VL53L0_RdByte(vl53l0_dev, 0xc3, &id);
	vl53l0_errmsg("read 0xc3: 0x%x\n", id);

	/* Read Model Version */
	VL53L0_RdByte(vl53l0_dev, 0xC0, &type);
	VL53L0_RdByte(vl53l0_dev, VL53L0_REG_IDENTIFICATION_REVISION_ID,
		&revision);
	VL53L0_RdByte(vl53l0_dev, 0xc3,
		&module_id);
	vl53l0_errmsg("STM VL53L0 Model type : %d. rev:%d. module:%d\n", type,
		revision, module_id);

	vl53l0_dev->I2cDevAddr = 0x52;
	vl53l0_dev->comms_type = 1;
	vl53l0_dev->comms_speed_khz = 400;

	/* Setup API functions based on revision */
	stmvl53l0_setupAPIFunctions(data);

	if (Status == VL53L0_ERROR_NONE && data->reset) {
		pr_err("Call of VL53L0_DataInit\n");
		Status = papi_func_tbl->DataInit(vl53l0_dev);
		data->reset = 0;
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("VL53L0_GetDeviceInfo:\n");
		Status = papi_func_tbl->GetDeviceInfo(vl53l0_dev, &DeviceInfo);
		if (Status == VL53L0_ERROR_NONE) {
			pr_err("Device Name : %s\n", DeviceInfo.Name);
			pr_err("Device Type : %s\n", DeviceInfo.Type);
			pr_err("Device ID : %s\n", DeviceInfo.ProductId);
			pr_err("ProductRevisionMajor : %d\n",
				DeviceInfo.ProductRevisionMajor);
			pr_err("ProductRevisionMinor : %d\n",
				DeviceInfo.ProductRevisionMinor);
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("Call of VL53L0_StaticInit\n");
		Status = papi_func_tbl->StaticInit(vl53l0_dev);
		/* Device Initialization */
	}

	if (Status == VL53L0_ERROR_NONE) {

		pr_err("Call of papi_func_tbl->SetDeviceMode\n");
		Status = papi_func_tbl->SetDeviceMode(vl53l0_dev,
			VL53L0_DEVICEMODE_SINGLE_RANGING);
		/* Setup in	single ranging mode */
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("set LimitCheckValue SIGMA_FINAL_RANGE\n");
		SigmaLimitValue = 32 << 16;
		Status = papi_func_tbl->SetLimitCheckValue(vl53l0_dev,
			VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
			SigmaLimitValue);
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("set LimitCheckValue SIGNAL_RATE_FINAL_RANGE\n");
		SigmaLimitValue = 94743; /* 1.44567500 * 65536 */
		Status = papi_func_tbl->SetLimitCheckValue(vl53l0_dev,
			VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
			SigmaLimitValue);
	}

	/*  Enable/Disable Sigma and Signal check */
	if (Status == VL53L0_ERROR_NONE)
		Status = papi_func_tbl->SetLimitCheckEnable(vl53l0_dev,
		VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, 1);

	if (Status == VL53L0_ERROR_NONE)
		Status = papi_func_tbl->SetLimitCheckEnable(vl53l0_dev,
		VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1);

	if (Status == VL53L0_ERROR_NONE)
		Status = papi_func_tbl->SetWrapAroundCheckEnable(vl53l0_dev, 1);

#ifdef CALIBRATION_FILE
	/*stmvl53l0_read_calibration_file(data);*/
#endif

	vl53l0_dbgmsg("End\n");

	return 0;
}

irqreturn_t laser_isr(int irq, void *dev)
{
	struct stmvl53l0_data *data = gp_vl53l0_data;

	vl53l0_dbgmsg("interrupt called");

	if (data->w_mode != OFF_MODE || data->d_mode > 0) {
		vl53l0_dbgmsg("have mode");
		data->interrupt_received = 1;
		schedule_delayed_work(&data->dwork, 0);
	}

	return IRQ_HANDLED;
}
static int stmvl53l0_start(struct stmvl53l0_data *data)
{
	int rc = 0;

	/* Power up */
	rc = pmodule_func_tbl->power_up(data->client_object, &data->reset);
	if (rc) {
		vl53l0_errmsg("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}
	/* init */
	rc = stmvl53l0_init_client(data);
	if (rc) {
		vl53l0_errmsg("%d, error rc %d\n", __LINE__, rc);
		pmodule_func_tbl->power_down(data->client_object);
		return -EINVAL;
	}

	data->enable_ps_sensor = 1;
	vl53l0_dbgmsg("End\n");

	return rc;
}

static int stmvl53l0_stop(struct stmvl53l0_data *data)
{
	int rc = 0;

	vl53l0_dbgmsg("Enter\n");
		VL53L0_StopMeasurement(data);

	/* clean interrupt */
	VL53L0_ClearInterruptMask(data, 0);
	/* cancel work handler */
	stmvl53l0_cancel_handler(data);
	/* power down */
	rc = pmodule_func_tbl->power_down(data->client_object);
	if (rc) {
		vl53l0_errmsg("%d, error rc %d\n", __LINE__, rc);
		return rc;
	}
	vl53l0_dbgmsg("End\n");


	return rc;
}

/*
 * I2C init/probing/exit functions
 */
static const struct file_operations stmvl53l0_ranging_fops = {
	.owner =			THIS_MODULE,
	.unlocked_ioctl =	stmvl53l0_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= stmvl53l0_compat_ioctl,
#endif
	.open =				stmvl53l0_open,
	.flush =			stmvl53l0_flush,
	.release =			stmvl53l0_close,
};

/*
static struct miscdevice stmvl53l0_ranging_dev = {
	.minor =	MISC_DYNAMIC_MINOR,
	.name =		"stmvl53l0_ranging",
	.fops =		&stmvl53l0_ranging_fops
};
*/

int stmvl53l0_setup(struct stmvl53l0_data *data, uint8_t type)
{
	int rc = 0;
	int irq;
	int gpio;

	vl53l0_dbgmsg("Enter\n");
	data->lowv = 40;
	data->highv = 45;
	if (type == CCI_BUS) {
		data->bus_type = CCI_BUS;
		data->client_object = &(data->cci_client_object);
		/* assign function table */
		pmodule_func_tbl = &stmvl53l0_module_func_tbl_cci;
		gpio = data->cci_client_object.gconf.cam_gpio_req_tbl[1].gpio;
	} else {
		data->bus_type = I2C_BUS;
		data->client_object = &(data->i2c_client_object);
		pmodule_func_tbl = &stmvl53l0_module_func_tbl_i2c;
		gpio = data->i2c_client_object.gconf.cam_gpio_req_tbl[1].gpio;
		data->lowv = data->i2c_client_object.lowv;
		data->highv = data->i2c_client_object.highv;
	}

	/* init interrupt */
	gpio_request(gpio, "vl6180_gpio_int");
	gpio_direction_input(gpio);
	irq = gpio_to_irq(gpio);

	if (irq < 0) {
		vl53l0_errmsg("%d,filed to map GPIO: %d to interrupt:%d\n",
			__LINE__, gpio, irq);
	} else {

		vl53l0_dbgmsg("register_irq:%d\n", irq);
/* IRQF_TRIGGER_FALLING- poliarity:0 IRQF_TRIGGER_RISNG - poliarty:1 */
		rc = request_irq(irq, laser_isr,
			IRQF_TRIGGER_FALLING ,
			"vl6180_lb_gpio_int", (void *)data);
		if (rc) {
			vl53l0_errmsg("%d, Could not allocate INT %d\n",
				__LINE__, rc);
				goto exit_free_irq;
		}
	}
	data->irq = irq;
	enable_irq_wake(irq);
	vl53l0_dbgmsg("interrupt is hooked\n");
	/* init mutex */
	mutex_init(&data->update_lock);
	mutex_init(&data->work_mutex);
	data->w_mode = OFF_MODE;
	/* init work handler */
	INIT_DELAYED_WORK(&data->dwork, stmvl53l0_work_handler);

	/* Register to Input Device */
	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);

		goto exit_free_irq;
	}
	set_bit(EV_ABS, data->input_dev_ps->evbit);
	/* range in cm*/
	input_set_abs_params(
		data->input_dev_ps, ABS_DISTANCE, 0, 76, 0, 0);
	/* tv_sec */
	input_set_abs_params(
		data->input_dev_ps, ABS_HAT0X, 0, 0xffffffff, 0, 0);
	/* tv_usec */
	input_set_abs_params(
		data->input_dev_ps, ABS_HAT0Y, 0, 0xffffffff, 0, 0);
	/* range in_mm */
	input_set_abs_params(
		data->input_dev_ps, ABS_HAT1X, 0, 765, 0, 0);
	/* error code change maximum to 0xff for more flexibility */
	input_set_abs_params(
		data->input_dev_ps, ABS_HAT1Y, 0, 0xff, 0, 0);
	/* rtnRate */
	input_set_abs_params(
		data->input_dev_ps, ABS_HAT2X, 0, 0xffffffff, 0, 0);
	/* rtn_amb_rate */
	input_set_abs_params(
		data->input_dev_ps, ABS_HAT2Y, 0, 0xffffffff, 0, 0);
	/* rtn_conv_time */
	input_set_abs_params(
		data->input_dev_ps, ABS_HAT3X, 0, 0xffffffff, 0, 0);
	/* dmax */
	input_set_abs_params(
		data->input_dev_ps, ABS_HAT3Y, 0, 0xffffffff, 0, 0);
	data->input_dev_ps->name = "STM VL53L0 proximity sensor";

	rc = input_register_device(data->input_dev_ps);
	if (rc) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_free_dev_ps;
	}

	/* Register sysfs hooks */
	data->range_kobj = kobject_create_and_add("range", kernel_kobj);
	if (!data->range_kobj) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);

		goto exit_unregister_dev_ps;
	}
	rc = sysfs_create_group(data->range_kobj, &stmvl53l0_attr_group);
	if (rc) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);

		goto exit_unregister_dev_ps_1;
	}

	/* to register as a misc device */
	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->dev_name = devname;
	data->miscdev.name = data->dev_name;
	data->miscdev.fops = &stmvl53l0_ranging_fops;

	if (misc_register(&data->miscdev) != 0)
		vl53l0_errmsg("%d,Couldn't register misc dev\n", __LINE__);

	/* init default value */
	data->enable_ps_sensor = 0;
	data->reset = 1;
	data->delay_ms = 0x3e8;	/* delay time to 1s */
	data->enableDebug = 0;
/*	data->client_object.power_up = 0; */

	vl53l0_dbgmsg("support ver. %s enabled\n", DRIVER_VERSION);
	vl53l0_dbgmsg("End");

	return 0;
exit_unregister_dev_ps_1:
	kobject_put(data->range_kobj);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);

exit_free_dev_ps:
	input_free_device(data->input_dev_ps);

exit_free_irq:

	free_irq(irq, data);
	kfree(data);
	return rc;
}

static int __init stmvl53l0_init(void)
{
	struct stmvl53l0_data *vl53l0_data = NULL;
	int ret = 0;

	vl53l0_dbgmsg("Enter\n");

	if (gp_vl53l0_data == NULL) {
		vl53l0_data =
		kzalloc(sizeof(struct stmvl53l0_data), GFP_KERNEL);
		if (!vl53l0_data) {
			vl53l0_errmsg("%d failed no memory\n", __LINE__);
			return -ENOMEM;
		}
		/* assign to global variable */
		gp_vl53l0_data = vl53l0_data;
	} else {
		vl53l0_data = gp_vl53l0_data;
	}
	/* assign function table */
	papi_func_tbl = &stmvl53l0_api_func_tbl;
	/* client specific init function */
	stmvl53l0_module_func_tbl_i2c.init();
	stmvl53l0_module_func_tbl_cci.init();

	if (ret) {
		kfree(vl53l0_data);
		gp_vl53l0_data = NULL;
		vl53l0_errmsg("%d failed with %d\n", __LINE__, ret);
	}
	vl53l0_dbgmsg("End\n");

	return ret;
}

static void __exit stmvl53l0_exit(void)
{
	vl53l0_dbgmsg("Enter\n");
#if 0
	if (gp_vl53l0_data) {
		input_unregister_device(gp_vl53l0_data->input_dev_ps);
		input_free_device(gp_vl53l0_data->input_dev_ps);
#ifdef USE_INT
		free_irq(data->irq, gp_vl53l0_data);
#endif
		sysfs_remove_group(
			gp_vl53l0_data->range_kobj, &stmvl53l0_attr_group);
		gp_vl53l0_data->pmodule_func_tbl->deinit(
			gp_vl53l0_data->client_object);
		kfree(gp_vl53l0_data);
		gp_vl53l0_data = NULL;
	}
#endif
	vl53l0_dbgmsg("End\n");
}


struct stmvl53l0_data *stmvl53l0_getobject(void)
{
	return gp_vl53l0_data;
}


MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

late_initcall(stmvl53l0_init);
module_exit(stmvl53l0_exit);