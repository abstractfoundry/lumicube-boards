/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_ENV_SENSOR_MODULE

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "afException.h"
#include "useful.h"
#include "afFieldProtocol.h"
#include "bme280.h"


typedef struct sBME280Driver {
    I2C_HandleTypeDef * i2c;
    uint32_t sampleDelayms;
    struct bme280_dev dev;
    float temperature;
    float pressure;
    float humidity;
} BME280BoardDriver;

static BME280BoardDriver thpDrv = {};

static uint8_t zero = 0;
static sExtraMetaDataFields temperatureMetaData  = {2, {{SETTABLE, &zero}, {UNITS, (uint8_t *) "celsius"}}};
static sExtraMetaDataFields pressureMetaData     = {2, {{SETTABLE, &zero}, {UNITS, (uint8_t *) "pascals"}}};
static sExtraMetaDataFields humidityMetaData     = {2, {{SETTABLE, &zero}, {UNITS, (uint8_t *) "percent"}}};

#ifndef THP_FIELDS_OFFSET
    #define THP_FIELDS_OFFSET 0
#endif

sFieldInfoEntry thpFieldTable[] = {
    { &thpDrv.temperature,   "temperature",   THP_FIELDS_OFFSET + 0,  1,  AF_FIELD_TYPE_FLOAT,   4,  0,  &temperatureMetaData, NULL, NULL},
    { &thpDrv.pressure,      "pressure",      THP_FIELDS_OFFSET + 1,  1,  AF_FIELD_TYPE_FLOAT,   4,  0,  &pressureMetaData, NULL, NULL},
    { &thpDrv.humidity,      "humidity",      THP_FIELDS_OFFSET + 2,  1,  AF_FIELD_TYPE_FLOAT,   4,  0,  &humidityMetaData, NULL, NULL},
};
const uint32_t thpFieldTableSize = sizeof(thpFieldTable)/sizeof(thpFieldTable[0]);


// ======================== //

void delay_us(uint32_t period, void *intf_ptr) {
    HAL_Delay(period/1000);
}

int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    // Return 0 for Success, non-zero for failure
    int8_t rslt = !(HAL_I2C_Mem_Read(thpDrv.i2c, BME280_I2C_ADDR_PRIM << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, reg_data, len, 10) == HAL_OK);
    return rslt;
}

int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    // Return 0 for Success, non-zero for failure
    int8_t rslt = !(HAL_I2C_Mem_Write(thpDrv.i2c, BME280_I2C_ADDR_PRIM << 1, reg_addr, I2C_MEMADD_SIZE_8BIT, (uint8_t *) reg_data, 1, 10) == HAL_OK);
    return rslt;
}

uint8_t testThp() {
    // If the board initialises then the I2C must be working
    return false;
}

void initialiseThp(I2C_HandleTypeDef * i2c) {
    addComponentFieldTableToGlobalTable(thpFieldTable, thpFieldTableSize);

    uint8_t dev_addr = BME280_I2C_ADDR_PRIM;
    struct bme280_dev * dev = &thpDrv.dev;
    thpDrv.i2c = i2c;
    dev->intf_ptr = &dev_addr;
    dev->intf = BME280_I2C_INTF;
    dev->read = user_i2c_read;
    dev->write = user_i2c_write;
    dev->delay_us = delay_us;

    int8_t rslt = bme280_init(dev); // 0 is success, >0 is warning, <0 is failure
    hardAssert(rslt == 0, "bme280_init warning or failure");

    // Oversampling - more -> better accuracy but higher current -> higher temperature
    // Go for lowest power
    dev->settings.osr_h = BME280_OVERSAMPLING_1X;
    dev->settings.osr_p = BME280_OVERSAMPLING_1X;
    dev->settings.osr_t = BME280_OVERSAMPLING_1X;
    dev->settings.filter = BME280_FILTER_COEFF_OFF;
    uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;
    rslt = bme280_set_sensor_settings(settings_sel, dev);
    hardAssert(rslt == 0, "bme280_set_sensor_settings warning or failure");

    // Calculate the minimum delay required between consecutive measurement based upon the sensor enabled
    // and the oversampling configuration.
    thpDrv.sampleDelayms = bme280_cal_meas_delay(&dev->settings);
}

void loopThp(uint32_t ticks) {
   static uint32_t lastTick = 0;
//    uint32_t startTick = HAL_GetTick();
    // loopAsserts(startTick	);

   if (ticks - lastTick > 100) {
       lastTick = ticks;

        // uint8_t transferId;
        // realCanBroadcast(100, 205, &transferId, 24, transferId, 1);



        struct bme280_dev * dev = &thpDrv.dev;
        int8_t rslt;

        // // First read just temperature 
        // rslt = bme280_set_sensor_mode(BME280_FORCED_MODE, dev);
        // softAssert(rslt == 0, "bme280_set_sensor_mode warning or failure");
        // HAL_Delay(thpDrv.sampleDelayms);
        // struct bme280_data data1;
        // rslt = bme280_get_sensor_data(BME280_ALL, &data1, &thpDrv.dev);
        // softAssert(rslt == 0, "bme280_get_sensor_data warning or failure");

        // // Now read everything
        // dev->settings.osr_h = BME280_OVERSAMPLING_1X;
        // dev->settings.osr_p = BME280_OVERSAMPLING_1X;
        // dev->settings.osr_t = BME280_OVERSAMPLING_1X;
        // dev->settings.filter = BME280_FILTER_COEFF_OFF;
        // uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;
        // rslt = bme280_set_sensor_settings(settings_sel, dev);
        // // Calculate the minimum delay required between consecutive measurement based upon the sensor enabled
        // // and the oversampling configuration.
        // thpDrv.sampleDelayms = bme280_cal_meas_delay(&dev->settings);


        rslt = bme280_set_sensor_mode(BME280_FORCED_MODE, dev);
        softAssert(rslt == 0, "bme280_set_sensor_mode warning or failure");
        HAL_Delay(thpDrv.sampleDelayms);
        struct bme280_data data2;
        rslt = bme280_get_sensor_data(BME280_ALL, &data2, &thpDrv.dev);
        softAssert(rslt == 0, "bme280_get_sensor_data warning or failure");

        // Publish results
        thpDrv.temperature = (float) data2.temperature / 100;
        thpDrv.humidity    = (float) data2.humidity    / 1000;
        thpDrv.pressure    = (float) data2.pressure    / 100;
        publishFieldsIfBelowBandwidth(0, 2);

        // // Setup for temperature reading next iteration
        // dev->settings.osr_h = BME280_NO_OVERSAMPLING;
        // dev->settings.osr_p = BME280_NO_OVERSAMPLING;
        // dev->settings.osr_t = BME280_OVERSAMPLING_1X;
        // dev->settings.filter = BME280_FILTER_COEFF_OFF;
        // settings_sel = BME280_OSR_TEMP_SEL | BME280_FILTER_SEL;
        // rslt = bme280_set_sensor_settings(settings_sel, dev);
        // // Calculate the minimum delay required between consecutive measurement based upon the sensor enabled
        // // and the oversampling configuration.
        // thpDrv.sampleDelayms = bme280_cal_meas_delay(&dev->settings);
   }

}

#endif // ENABLE_ENV_SENSOR_MODULE

