/* Copyright (c) 2020 Abstract Foundry Limited */
#include "project.h"
#ifdef ENABLE_LIGHT_MODULE

#include <stdint.h>
#include "apds9960I2C.h"
#include "useful.h"
#include "afException.h"

#define APDS9960_I2C_ADDR       0x39

typedef struct tapds9960FieldInfo {
	uint8_t address;
	uint8_t startBit;
	uint8_t endBit;
} apds9960FieldInfo;

static const apds9960FieldInfo apds9960FieldInfoTable[] = {
	[ENABLE_PON]         = {0x80, 0, 0},
	[ENABLE_AEN]         = {0x80, 1, 1},
	[ENABLE_PEN]         = {0x80, 2, 2},
	[ENABLE_WEN]         = {0x80, 3, 3},
	[ENABLE_AIEN]        = {0x80, 4, 4},
	[ENABLE_PIEN]        = {0x80, 5, 5},
	[ENABLE_GEN]         = {0x80, 6, 6},
	[ATIME]              = {0x81, 0, 7},
	[WTIME]              = {0x83, 0, 7},
	[AILTL]              = {0x84, 0, 7},
	[AILTH]              = {0x85, 0, 7},
	[AIHTL]              = {0x86, 0, 7},
	[AIHTH]              = {0x87, 0, 7},
	[PILT]               = {0x89, 0, 7},
	[PIHT]               = {0x8B, 0, 7},
	[PERS]               = {0x8C, 0, 7},
	[CONFIG1_WLONG]      = {0x8D, 1, 1},
	[PPULSE_PPLEN]       = {0x8E, 6, 7},
	[PPULSE_PPULSE]      = {0x8E, 0, 5},
	[CONTROL_LDRIVE]     = {0x8F, 6, 7},
	[CONTROL_PGAIN]      = {0x8F, 2, 3},
	[CONTROL_AGAIN]      = {0x8F, 0, 1},
	[CONFIG2_PSIEN]      = {0x90, 7, 7},
	[CONFIG2_CPSIEN]     = {0x90, 6, 6},
	[CONFIG2_LED_BOOST]  = {0x90, 4, 5},
	[ID]                 = {0x92, 0, 7},
	[STATUS_CPSAT]       = {0x93, 7, 7},
	[STATUS_PGSAT]       = {0x93, 6, 6},
	[STATUS_PINT]        = {0x93, 5, 5},
	[STATUS_AINT]        = {0x93, 4, 4},
	[STATUS_GINT]        = {0x93, 2, 2},
	[STATUS_PVALID]      = {0x93, 1, 1},
	[STATUS_AVALID]      = {0x93, 0, 0},
	[CDATAL]             = {0x94, 0, 7},
	[CDATAH]             = {0x95, 0, 7},
	[RDATAL]             = {0x96, 0, 7},
	[RDATAH]             = {0x97, 0, 7},
	[GDATAL]             = {0x98, 0, 7},
	[GDATAH]             = {0x99, 0, 7},
	[BDATAL]             = {0x9A, 0, 7},
	[BDATAH]             = {0x9B, 0, 7},
	[PDATA]              = {0x9C, 0, 7},
	[POFFSET_UR]         = {0x9D, 0, 7},
	[POFFSET_DL]         = {0x9E, 0, 7},
	[CONFIG3]            = {0x9F, 0, 7},
	[GPENTH]             = {0xA0, 0, 7},
	[GEXTH]              = {0xA1, 0, 7},
	[GCONF1_GFIFOTH]     = {0xA2, 6, 7},
	[GCONF1_GEXMSK]      = {0xA2, 2, 5},
	[GCONF1_GEXPERS]     = {0xA2, 0, 1},
	[GCONF2_GGAIN]       = {0xA3, 5, 6},
	[GCONF2_GLDRIVE]     = {0xA3, 3, 4},
	[GCONF2_GWTIME]      = {0xA3, 0, 2},
	[GOFFSET_U]          = {0xA4, 0, 7},
	[GOFFSET_D]          = {0xA5, 0, 7},
	[GOFFSET_L]          = {0xA7, 0, 7},
	[GOFFSET_R]          = {0xA9, 0, 7},
	[GPULSE_GPLEN]       = {0xA6, 6, 7},
	[GPULSE_GPULSE]      = {0xA6, 0, 5},
	[GCONF3_GDIMS]       = {0xAA, 0, 1},
	[GCONF4_GFIFO_CLR]   = {0xAB, 2, 2},
	[GCONF4_GIEN]        = {0xAB, 1, 1},
	[GCONF4_GMODE]       = {0xAB, 0, 0},
	[GFLVL]              = {0xAE, 0, 7},
	[GSTATUS_GFOV]       = {0xAF, 1, 1},
	[GSTATUS_GVALID]     = {0xAF, 0, 0},
	[IFORCE]             = {0xE4, 0, 7},
	[PICLEAR]            = {0xE5, 0, 7},
	[CICLEAR]            = {0xE6, 0, 7},
	[AICLEAR]            = {0xE7, 0, 7},
	[GFIFO_U]            = {0xFC, 0, 7},
	[GFIFO_D]            = {0xFD, 0, 7},
	[GFIFO_L]            = {0xFE, 0, 7},
	[GFIFO_R]            = {0xFF, 0, 7},
};

typedef struct tapds9960ReservedFieldInfo {
	uint8_t address;
	uint8_t startBit;
	uint8_t endBit;
	uint8_t data;
} apds9960ReservedFieldInfo;

void i2cRead(I2C_HandleTypeDef * hi2c, uint8_t memAddress, uint8_t * data, uint16_t length) {
	for (uint8_t i=0; i<10; i++) {
		if (HAL_I2C_Mem_Read(hi2c, APDS9960_I2C_ADDR << 1, memAddress, I2C_MEMADD_SIZE_8BIT, data, length, 10) == HAL_OK)
			return;
	}
	hardAssert(0, "Read failed");
}

void i2cWriteByte(I2C_HandleTypeDef * hi2c, uint8_t memAddress, uint8_t data) {
	for (uint8_t i=0; i<10; i++) {
		if (HAL_I2C_Mem_Write(hi2c, APDS9960_I2C_ADDR << 1, memAddress, I2C_MEMADD_SIZE_8BIT, &data, 1, 10) == HAL_OK)
			return;
	}
	hardAssert(0, "Write failed");
}

uint8_t i2cReadByte(I2C_HandleTypeDef * hi2c, uint8_t memAddress) {
	uint8_t data;
	i2cRead(hi2c, memAddress, &data, 1);
	return data;
}

void i2cReadModifyWriteBits(I2C_HandleTypeDef * hi2c, uint8_t memAddress, uint8_t data, uint8_t startBit, uint8_t endBit) {
	uint8_t regData = i2cReadByte(hi2c, memAddress);
	uint8_t origRegData = regData;
	modifyBitsU8(&regData, data, startBit, endBit);
	if (origRegData != regData)
		i2cWriteByte(hi2c, memAddress, regData);
}

void i2cWriteField(I2C_HandleTypeDef * hi2c, apds9960FieldEnum field, uint8_t data) {
	apds9960FieldInfo fieldInfo = apds9960FieldInfoTable[field];
	if (fieldInfo.startBit == 0 && fieldInfo.endBit == 7) {
		i2cWriteByte(hi2c, fieldInfo.address, data);
	} else {
		i2cReadModifyWriteBits(hi2c, fieldInfo.address, data, fieldInfo.startBit, fieldInfo.endBit);
	}
}

uint8_t i2cReadField(I2C_HandleTypeDef * hi2c, apds9960FieldEnum field) {
	apds9960FieldInfo fieldInfo = apds9960FieldInfoTable[field];
	uint8_t data = i2cReadByte(hi2c, fieldInfo.address);
	if (fieldInfo.startBit == 0 && fieldInfo.endBit == 7) {
		return data;
	} else {
		return getBitsU8(data, fieldInfo.startBit, fieldInfo.endBit);
	}
}

// Write multiple fields belonging to the same register
void i2cWriteFields(I2C_HandleTypeDef * hi2c, apds9960FieldEnum * fields, uint8_t numFields, uint8_t * data) {
	uint8_t regAddress = apds9960FieldInfoTable[fields[0]].address;
	uint8_t regData = i2cReadByte(hi2c, regAddress);
	uint8_t origRegData = regData;
	for (uint8_t i=0; i<numFields; i++) {
		apds9960FieldInfo fieldInfo = apds9960FieldInfoTable[fields[i]];
		hardAssert(fieldInfo.address == regAddress, "All fields must be in the same register\r\n");
		modifyBitsU8(&regData, data[i], fieldInfo.startBit, fieldInfo.endBit);
	}
	if (regData != origRegData)
		i2cWriteByte(hi2c, regAddress, regData);
}

// Read multiple fields belonging to the same register
void i2cReadFields(I2C_HandleTypeDef * hi2c, apds9960FieldEnum * fields, uint8_t numFields, uint8_t * data) {
	uint8_t regAddress = apds9960FieldInfoTable[fields[0]].address;
	uint8_t regData = i2cReadByte(hi2c, regAddress);
	for (uint8_t i=0; i<numFields; i++) {
		apds9960FieldInfo fieldInfo = apds9960FieldInfoTable[fields[i]];
		hardAssert(fieldInfo.address == regAddress, "All fields must be in the same register\r\n");
		data[i] = getBitsU8(regData, fieldInfo.startBit, fieldInfo.endBit);
	}
}

/* Functions to read/write special registers */

// Data must be read all at once in this order
void i2cReadARGBRegisters(I2C_HandleTypeDef * hi2c, uint16_t * ambientLight, uint16_t * red, uint16_t * green, uint16_t * blue) {
	uint8_t data[8];
	uint8_t address = apds9960FieldInfoTable[CDATAL].address;
	i2cRead(hi2c, address, &data[0], 8);
	*ambientLight = (data[1] << 8) | data[0];
	*red          = (data[3] << 8) | data[2];
	*green        = (data[5] << 8) | data[4];
	*blue         = (data[7] << 8) | data[6];
}

// Data must be read all at once in this order
void i2cReadGestureFifo(I2C_HandleTypeDef * hi2c, uint8_t * up, uint8_t * down, uint8_t * left, uint8_t * right) {
	uint8_t data[4];
	uint8_t address = apds9960FieldInfoTable[GFIFO_U].address;
	i2cRead(hi2c, address, &data[0], 4);
	*up = data[0];
	*down = data[1];
	*left = data[2];
	*right = data[3];
}

// These bits are all in the same register
void setEnableBits(I2C_HandleTypeDef * hi2c, uint8_t colour, uint8_t proximity, uint8_t gesture) {
	apds9960FieldEnum fields[] = {ENABLE_AEN, ENABLE_PEN, ENABLE_GEN};
	uint8_t values[] = {colour, proximity, gesture};
	i2cWriteFields(hi2c, &fields[0], 3, values);
}

#endif //ENABLE_LIGHT_SENSOR
