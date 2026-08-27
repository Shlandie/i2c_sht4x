idf_component_register(SRCS "i2c_sht4x.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c esp_timer)

