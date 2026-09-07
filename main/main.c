#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "freertos/projdefs.h"
#include "i2c_sht4x.h"

#define I2C_PORT_1			1


sht4x_i2c_master_bus_ctx_t master_bus = {0};
sht4x_t device = {0};

int8_t temperature, humidity;

static void initilization()
{
	i2c_master_bus_config_t master_bus_cfg = 
	{
		.i2c_port 		= I2C_PORT_1,
		.clk_source 	= I2C_CLK_SRC_XTAL,
		.scl_io_num		= GPIO_NUM_5,
		.sda_io_num		= GPIO_NUM_4
	};
		
	sht4x_i2c_master_bus_init(&master_bus, master_bus_cfg);
	sht4x_i2c_device_init(&master_bus, &device, SHT4X_ADDR_1, STANDARD, false);
}

void app_main(void)
{
	vTaskDelay(pdMS_TO_TICKS(500));
	initilization();
	
	while(1)
	{
		sht4x_measure(&device);
		sht4x_read(&device, &temperature, &humidity);
		printf("Temperature: %dC, Humidity: %d%%", temperature, humidity);
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}
