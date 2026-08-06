#include <stdio.h>

#include "i2c_sht4x.h"
#include "freertos/idf_additions.h"


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
	device.master_bus_mutex = &master_bus_dev->master_bus_mutex;
	
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