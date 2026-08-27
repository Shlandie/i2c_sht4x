#include <stdio.h>

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


static const char TAG[] = "I2C_SHT4X";


static void sht4x_restore_access(void *arg)
{
	sht4x_t *device = (sht4x_t* ) arg;
	xSemaphoreGive(device->device_access_mutex);
	esp_timer_delete(device->timer);
}


sht4x_i2c_master_bus_ctx_t sht4x_i2c_master_bus_init(i2c_master_bus_config_t master_bus_config)
{
	sht4x_i2c_master_bus_ctx_t master_bus = {0};
	
	i2c_new_master_bus(&master_bus_config, &master_bus.master_bus_handle);
	master_bus.master_bus_mutex = xSemaphoreCreateMutex();
	
	return master_bus;
}

esp_err_t sht4x_i2c_device_init(sht4x_i2c_master_bus_ctx_t *master_bus_dev, sht4x_t *device_desc,  sht4x_scl_adress_t device_addr, sht4x_scl_speed_t speed_mode, bool disable_ack_check)
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
	ret = i2c_master_bus_add_device(master_bus_dev->master_bus_handle, &dev_config, &(device_desc->dev_handle));
	ESP_RETURN_ON_ERROR(ret, TAG, "SHT4X I2C device init failed");
	
	// Get port mutex on device descriptor for easier access and create mutex for device access
	device_desc->master_bus_mutex = master_bus_dev->master_bus_mutex;
	device_desc->device_access_mutex = xSemaphoreCreateMutex();
	
	// Create timer which callbacks to give the device access mutex back
	const esp_timer_create_args_t timer_args = {
		.name =  "sht4x_restore_access",
		.dispatch_method = ESP_TIMER_TASK,
		.callback = sht4x_restore_access,
		.arg = device_desc
	};
	ret = esp_timer_create(&timer_args, &(device_desc->timer));
	ESP_RETURN_ON_ERROR(ret, TAG, "SHT4X device timer creation for access mutex managment failed");
	
	
	ESP_LOGI(TAG, "SHT4X device initialized successfully");	
	return ret;
}

esp_err_t sht4x_reset_device(sht4x_t *device)
{
	// esp_err_t for ESP error handling macros
	esp_err_t ret = ESP_OK;
	
	// Take current device semaphore
	xSemaphoreTake(device->device_access_mutex, pdMS_TO_TICKS(SHT4X_DEVICE_MUTEX_TIMEOUT));
	
	const uint8_t cmd = CMD_SOFT_RESET;	
	// Take port mutex which the current device is on. Send the reset command
	xSemaphoreTake(device->master_bus_mutex, pdMS_TO_TICKS(SHT4X_MASTER_MUTEX_TIMEOUT));
	ESP_GOTO_ON_ERROR(i2c_master_transmit(device->dev_handle, &cmd, CMD_LENGTH, SHT4X_TRANSACTION_TIMEOUT), cleanup, TAG, "I2C SOFT-RESET CMD TRANSMISSION FAILED");
	xSemaphoreGive(device->master_bus_mutex);
	
	ESP_LOGI(TAG, "SHT4X device (soft) reset");
	
	// Create timer for callback which returns access (gives device mutex back) to the device after a safe period has elapsed
	ESP_RETURN_ON_ERROR(esp_timer_start_once(device->timer, SOFT_RESET_TIMEOFF), TAG, "Failed to start timer for device access mutex restore");
	
	return ESP_OK;
	
	// If transmission or timer callback creation fails, device timeout is not needed. Return the device mutex
	cleanup:
	xSemaphoreGive(device->device_access_mutex);
	return ret;
		
}