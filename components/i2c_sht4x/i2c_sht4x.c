#include <stdio.h>

#include "i2c_sht4x.h"


void sht4x_i2c_device_init(i2c_master_bus_handle_t master_bus_handle, sht4x_t device_dev, sht4x_scl_adress_t device_addr, sht4x_scl_speed_t speed_mode, bool disable_ack_check)
{
	i2c_device_config_t dev_config = {
		.dev_addr_length 			= I2C_ADDR_BIT_LEN_7,
		.device_address				= device_addr,
		.scl_speed_hz				= speed_mode,
		.scl_wait_us				= 0,
		.flags.disable_ack_check 	= disable_ack_check 
	};
	
	i2c_master_bus_add_device(master_bus_handle, &dev_config, &device_dev.dev_handle);
}