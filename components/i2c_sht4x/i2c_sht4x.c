#include <stdio.h>

#include "esp_log.h"
#include "freertos/projdefs.h"
#include "i2c_sht4x.h"
#include "freertos/idf_additions.h"

#define CMD_LENGTH					1
#define CMD_RESET					0x94
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


static const char TAG[] = "I2C_SHT4X";

sht4x_i2c_master_bus_ctx_t sht4x_i2c_master_bus_init(i2c_master_bus_config_t master_bus_config)
{
	sht4x_i2c_master_bus_ctx_t master_bus = {0};
	
	i2c_new_master_bus(&master_bus_config, &master_bus.master_bus_handle);
	master_bus.master_bus_mutex = xSemaphoreCreateMutex();
	
	return master_bus;
}

sht4x_t sht4x_i2c_device_init(sht4x_i2c_master_bus_ctx_t *master_bus_dev, sht4x_scl_adress_t device_addr, sht4x_scl_speed_t speed_mode, bool disable_ack_check)
{
	sht4x_t device = {0};
	device.master_bus_mutex = master_bus_dev->master_bus_mutex;
	
	i2c_device_config_t dev_config = {
		.dev_addr_length 			= I2C_ADDR_BIT_LEN_7,
		.device_address				= device_addr,
		.scl_speed_hz				= speed_mode,
		.scl_wait_us				= 0,
		.flags.disable_ack_check 	= disable_ack_check 
	};
	i2c_master_bus_add_device(master_bus_dev->master_bus_handle, &dev_config, &device.dev_handle);
	
	return device;
}

void sht4x_reset_device(sht4x_t *device)
{
	uint8_t cmd = CMD_RESET; 
	
	xSemaphoreTake(device->master_bus_mutex, pdMS_TO_TICKS(SHT4X_MUTEX_TIMEOUT));
	i2c_master_transmit(device->dev_handle, &cmd, CMD_LENGTH, SHT4X_TRANSACTION_TIMEOUT);
	xSemaphoreGive(device->master_bus_mutex);
	
	ESP_LOGI(TAG, "SHT4X device (soft) reset");
}