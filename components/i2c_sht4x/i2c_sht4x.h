#pragma once

#include "driver/i2c_master.h"

#define CMD_RESET             0x94
#define CMD_SERIAL            0x89
#define CMD_MEAS_HIGH         0xfd
#define CMD_MEAS_MED          0xf6
#define CMD_MEAS_LOW          0xe0
#define CMD_MEAS_H_HIGH_LONG  0x39
#define CMD_MEAS_H_HIGH_SHORT 0x32
#define CMD_MEAS_H_MED_LONG   0x2f
#define CMD_MEAS_H_MED_SHORT  0x24
#define CMD_MEAS_H_LOW_LONG   0x1e
#define CMD_MEAS_H_LOW_SHORT  0x15


typedef struct sht4x
{
}sht4x_t;