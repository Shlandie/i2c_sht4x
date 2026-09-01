#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "i2c_sht4x.h"
#include "freertos/idf_additions.h"

#define CMD_LENGTH					1
#define CMD_SOFT_RESET				0x94
#define CMD_SERIAL           	 	0x89
#define CMD_MEAS_HIGH        	 	0xfd
#define CMD_MEAS_MED          		0xf6
#define CMD_MEAS_LOW         	 	0xe0
#define CMD_MEAS_H_HIGH_LONG  		0x39
#define CMD_MEAS_H_HIGH_SHORT 		0x32
#define CMD_MEAS_H_MED_LONG   		0x2f
#define CMD_MEAS_H_MED_SHORT  		0x24
#define CMD_MEAS_H_LOW_LONG   		0x1e
#define CMD_MEAS_H_LOW_SHORT  		0x15

#define CHECK(x) do { esp_err_t __; if ((__ = x) != ESP_OK) return __; } while (0)
#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)


// Used together with device_access_mutex in sht4x_t to deny access for specific periods when the sensor is measuring, soft-resetting (IN MICROSECONDS)
typedef enum
{
	SOFT_RESET_TIMEOFF			= 1000,
	LOW_REPEAT_TIMEOFF			= 1600,
	MEDIUM_REPEAT_TIMEOFF		= 4500,
	HIGH_REPEAT_TIMEOFF			= 8300
}sht4x_access_timeoff_t;


static const char TAG[] = "I2C_SHT4X";


static inline sht4x_access_timeoff_t get_access_restrict_time(sht4x_t *device_desc)
{
	if (device_desc->heater == SHT4X_HEATER_OFF)
	{
		switch(device_desc->repeatability)
		{
			case SHT4X_REPEAT_HIGH:
				return HIGH_REPEAT_TIMEOFF;
			case SHT4X_REPEAT_MEDIUM:
				return MEDIUM_REPEAT_TIMEOFF;
			case SHT4X_REPEAT_LOW:
				return LOW_REPEAT_TIMEOFF;
		}
	}
	else
	{
		return HIGH_REPEAT_TIMEOFF;
	}	
}

static inline uint8_t get_cmd(sht4x_t *device_desc)
{
		switch (device_desc->heater)
		{
			case SHT4X_HEATER_OFF:
				switch (device_desc->repeatability)
				{
					case SHT4X_REPEAT_HIGH:
						return CMD_MEAS_HIGH;
					case SHT4X_REPEAT_MEDIUM:
						return CMD_MEAS_MED;
					case SHT4X_REPEAT_LOW:
						return CMD_MEAS_LOW;
				}
			case SHT4X_HEATER_HIGH_LONG:
				return CMD_MEAS_H_HIGH_LONG;
			case SHT4X_HEATER_HIGH_SHORT:
				return CMD_MEAS_H_HIGH_SHORT;
			case SHT4X_HEATER_MEDIUM_LONG:
				return CMD_MEAS_H_MED_LONG;
			case SHT4X_HEATER_MEDIUM_SHORT:
				return CMD_MEAS_H_MED_SHORT;
			case SHT4X_HEATER_LOW_LONG:
				return CMD_MEAS_H_LOW_LONG;
			case SHT4X_HEATER_LOW_SHORT:
				return CMD_MEAS_H_LOW_SHORT;
		}	
}

static void restore_device_access(void *arg)
{
	sht4x_t *device = (sht4x_t* ) arg;
	xSemaphoreGive(device->device_access_mutex);
}


esp_err_t sht4x_i2c_master_bus_init(sht4x_i2c_master_bus_ctx_t *master_bus_ctx, i2c_master_bus_config_t master_bus_config)
{
	esp_err_t ret = ESP_OK;
	
	memset(master_bus_ctx, 0, sizeof(sht4x_i2c_master_bus_ctx_t));

	ret = i2c_new_master_bus(&master_bus_config, &(master_bus_ctx->master_bus_handle));
	ESP_RETURN_ON_ERROR(ret, TAG, "I2C PORT %d INIT FAILED", master_bus_config.i2c_port);
	
	master_bus_ctx->master_bus_mutex = xSemaphoreCreateMutex();
	
	return ret;
}

esp_err_t sht4x_i2c_device_init(sht4x_i2c_master_bus_ctx_t *master_bus_ctx, sht4x_t *device_desc,  sht4x_scl_adress_t device_addr, sht4x_scl_speed_t speed_mode, bool disable_ack_check)
{
	esp_err_t ret = ESP_OK;
	
	// Initialize SHT4X device on the given I2C port
	i2c_device_config_t dev_config = {
		.dev_addr_length 			= I2C_ADDR_BIT_LEN_7,
		.device_address				= device_addr,
		.scl_speed_hz				= speed_mode,
		.scl_wait_us				= 0,
		.flags.disable_ack_check 	= disable_ack_check 
	};
	ret = i2c_master_bus_add_device(master_bus_ctx->master_bus_handle, &dev_config, &(device_desc->dev_handle));
	ESP_RETURN_ON_ERROR(ret, TAG, "SHT4X I2C DEVICE INIT FAILED");
	
	// Get port mutex on device descriptor for easier access and create mutex for device access
	device_desc->master_bus_mutex = master_bus_ctx->master_bus_mutex;
	device_desc->device_access_mutex = xSemaphoreCreateMutex();
	
	// Create timer which callbacks to give the device access mutex back
	const esp_timer_create_args_t timer_args = {
		.name =  "sht4x_restore_access",
		.dispatch_method = ESP_TIMER_TASK,
		.callback = restore_device_access,
		.arg = device_desc
	};
	ret = esp_timer_create(&timer_args, &(device_desc->timer));
	ESP_RETURN_ON_ERROR(ret, TAG, "SHT4X DEVICE TIMER CREATION FOR ACCESS MUTEX MANAGMENT FAILED");
	
	
	ESP_LOGI(TAG, "SHT4X device initialized successfully");	
	return ret;
}

esp_err_t sht4x_reset_device(sht4x_t *device_desc)
{
	// Take current device semaphore
	xSemaphoreTake(device_desc->device_access_mutex, pdMS_TO_TICKS(SHT4X_DEVICE_MUTEX_TIMEOUT));
	
	// esp_err_t for ESP error handling macros
	esp_err_t ret = ESP_OK;
	
	// Take port mutex which the current device is on. Send the reset command
	const uint8_t cmd = CMD_SOFT_RESET;	
	xSemaphoreTake(device_desc->master_bus_mutex, pdMS_TO_TICKS(SHT4X_MASTER_MUTEX_TIMEOUT));
	ret = i2c_master_transmit(device_desc->dev_handle, &cmd, CMD_LENGTH, SHT4X_TRANSACTION_TIMEOUT);
	ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "I2C SOFT-RESET CMD TRANSMISSION FAILED");
	xSemaphoreGive(device_desc->master_bus_mutex);
	
	ESP_LOGI(TAG, "SHT4X device (soft) reset command sent");
	
	// Create timer for callback which returns access (gives device access mutex back) to the device after a safe period has elapsed
	ret = esp_timer_start_once(device_desc->timer, SOFT_RESET_TIMEOFF);
	ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "FAILED TO START TIMER FOR DEVICE ACCESS MUTEX RESTORE (ON SOFT-RESET). DON'T ACCESS DEVICE FOR ATLEAST %d SECOND(S)", SOFT_RESET_TIMEOFF);
	
	return ret;
	
	// If transmission or timer callback creation fails, device timeout is not needed. Return the device mutex
	cleanup:
	xSemaphoreGive(device_desc->master_bus_mutex);
	xSemaphoreGive(device_desc->device_access_mutex);
	return ret;	
}

esp_err_t sht4x_measure(sht4x_t *device_desc)
{
	// Take current device semaphore
	xSemaphoreTake(device_desc->device_access_mutex, pdMS_TO_TICKS(SHT4X_DEVICE_MUTEX_TIMEOUT));
	
	// esp_err_t for ESP error handling macros
	esp_err_t ret = ESP_OK;
	
	// Take port mutex which the current device is on. Send the measure command
	uint8_t cmd = get_cmd(device_desc);
	xSemaphoreTake(device_desc->master_bus_mutex, pdMS_TO_TICKS(SHT4X_MASTER_MUTEX_TIMEOUT));
	ret = i2c_master_transmit(device_desc->dev_handle, &cmd, CMD_LENGTH, SHT4X_TRANSACTION_TIMEOUT);
	ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "I2C MEASURE CMD TRANSMISSION FAILED");
	xSemaphoreGive(device_desc->master_bus_mutex);
	
	ESP_LOGI(TAG, "SHT4X device measurement command sent");
	
	// Create timer for callback which returns access (gives device access mutex back) to the device after a safe period has elapsed
	sht4x_access_timeoff_t delay = get_access_restrict_time(device_desc);
	ret = esp_timer_start_once(device_desc->timer, delay);
	ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "FAILED TO START TIMER FOR DEVICE ACCESS MUTEX RESTORE (ON MEASURE). DON'T ACCESS DEVICE FOR ATLEAST %d SECOND(S)", delay);
	
	return ret;
	
	cleanup:
	xSemaphoreGive(device_desc->master_bus_mutex);
	xSemaphoreGive(device_desc->device_access_mutex);
	return ret;	
}

esp_err_t sht4x_read(sht4x_t *device_desc, uint8_t *temperature, uint8_t humidity)
{
	// Take current device semaphore
	xSemaphoreTake(device_desc->device_access_mutex, pdMS_TO_TICKS(SHT4X_DEVICE_MUTEX_TIMEOUT));
		
	// esp_err_t for ESP error handling macros
	esp_err_t ret = ESP_OK;
	
	// Take port mutex which the current device is on. Send I2C read
	uint8_t read_buffer[4] = {0};
	xSemaphoreTake(device_desc->master_bus_mutex, pdMS_TO_TICKS(SHT4X_MASTER_MUTEX_TIMEOUT));
	ret = i2c_master_receive(device_desc->dev_handle, read_buffer, 4, SHT4X_TRANSACTION_TIMEOUT);
	ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "I2C READ FAILED");
	xSemaphoreGive(device_desc->master_bus_mutex);
	
	
	
	// Give back device access semaphore and return
	xSemaphoreGive(device_desc->device_access_mutex);
	return ret;	
	
	cleanup:
	xSemaphoreGive(device_desc->master_bus_mutex);
	xSemaphoreGive(device_desc->device_access_mutex);
	return ret;	
}


