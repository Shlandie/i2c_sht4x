#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#include "freertos/projdefs.h"
#include "i2c_sht4x.h"


#define I2C_PORT_1			1


sht4x_i2c_master_bus_ctx_t master_bus = {0};
sht4x_t device = {0};

int32_t temperature, humidity;
float temperature_f, humidity_f;


static inline void get_whole_and_fraction(int32_t full_value, int8_t *whole, int8_t *fraction)
{
	*whole = full_value / SHT4X_INTEGER_PRECISION;
	
	int32_t remainder = full_value % SHT4X_INTEGER_PRECISION;
	
	int32_t fraction_precision = SHT4X_INTEGER_PRECISION / 100;
	*fraction = remainder / fraction_precision;
	
	if (*fraction < 0)
    {
        *fraction = -(*fraction);
    }
}

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

	int8_t whole_temp = 0, fraction_temp = 0;
	int8_t whole_humid = 0, fraction_humid = 0;
	
	while(1)
	{
		// Integers only
		sht4x_measure(&device);
		sht4x_read(&device, &temperature, &humidity);
		get_whole_and_fraction(temperature, &whole_temp, &fraction_temp);
		get_whole_and_fraction(humidity, &whole_humid, &fraction_humid);
		printf("Temperature: %" PRId8 ".%" PRId8 "C Humidity: %" PRId8 ".%" PRId8 "%%", whole_temp, fraction_temp, whole_humid, fraction_humid);
		vTaskDelay(pdMS_TO_TICKS(1000));
		
		// Uses floats
		sht4x_measure(&device);
		sht4x_read_float(&device, &temperature_f, &humidity_f);
		printf("Temperature: %fC, Humidity: %f%%", temperature_f, humidity_f);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
