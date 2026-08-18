#pragma once


#include "driver/i2c_master.h"
#include "freertos/idf_additions.h"
#include "hal/i2c_types.h"


#define SHT4X_MUTEX_TIMEOUT				100
#define SHT4X_TRANSACTION_TIMEOUT		100


typedef enum
{
	SHT4X_ADDR_1 = 0x44,
	SHT4X_ADDR_2 = 0x45,
	SHT4X_ADDR_3 = 0x46
}sht4x_scl_adress_t;

typedef enum
{
	STANDARD 		= 100000,
	FAST_MODE 		= 400000,
	FAST_MODE_PLUS 	= 1000000
}sht4x_scl_speed_t;


typedef enum
{
    SHT4X_HEATER_OFF = 0,      /**< Heater is off, default */
    SHT4X_HEATER_HIGH_LONG,    /**< High power (~200mW), 1 second pulse */
    SHT4X_HEATER_HIGH_SHORT,   /**< High power (~200mW), 0.1 second pulse */
    SHT4X_HEATER_MEDIUM_LONG,  /**< Medium power (~110mW), 1 second pulse */
    SHT4X_HEATER_MEDIUM_SHORT, /**< Medium power (~110mW), 0.1 second pulse */
    SHT4X_HEATER_LOW_LONG,     /**< Low power (~20mW), 1 second pulse */
    SHT4X_HEATER_LOW_SHORT,    /**< Low power (~20mW), 0.1 second pulse */
} sht4x_heater_t;


typedef struct i2c_master_bus
{
	i2c_master_bus_handle_t master_bus_handle;
	SemaphoreHandle_t master_bus_mutex;				// Mutex for the port
}sht4x_i2c_master_bus_ctx_t;

typedef struct sht4x
{
	i2c_master_dev_handle_t dev_handle;					
	SemaphoreHandle_t master_bus_mutex;				// Place to hold the mutex for the port the device uses
	
	sht4x_heater_t heater;							// 
}sht4x_t;


/*
* Initialize I2C master bus handle and create mutex for this exact master bus
* @param i2c_master_bus_config_t	Configuration struct of the master bus
* @return sht4x_i2c_master_bus_ctx_t
*/
sht4x_i2c_master_bus_ctx_t sht4x_i2c_master_bus_init(i2c_master_bus_config_t master_bus_config);

/*
* Initialize I2C slave device according to its constraints
* @param master_bus_handle		Handle to the I2C master bus
* @param device_addr 			SHT4x addr
* @param speed_mode  			I2C speed mode of this master and slave communication
* @param disable_ack_check		Disable ACK check. If this is set false, that means ack check is enabled, the transaction will be stopped and API returns error when nack is detected.		
* @return sht4x_t				Device descriptor
*/
sht4x_t sht4x_i2c_device_init(sht4x_i2c_master_bus_ctx_t *master_bus, sht4x_scl_adress_t device_addr, sht4x_scl_speed_t speed_mode, bool disable_ack_check);

/*
 * Soft resets the sht4x device
 * @param device		device descriptor
 */
esp_err_t sht4x_reset_device(sht4x_t *device);

/*
 * Read and return temperature and humidity in integer format. Turn on heater if a mode is specified in the descriptor
 * @param device		device desriptor
 * @param temperature	
 */
esp_err_t sht4x_measure(sht4x_t);






