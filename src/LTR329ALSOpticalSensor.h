#pragma once

///////////////////////////////////////////////////////////////////////////////
// LiteOn LTR-329ALS-01 Optical Sensor with I2C Interface
// Copyright (C) muman.ch + github.com/mumanchu, 2026.09.24
// If you re-use this code, please include the copyright notice above
// info@muman.ch
// https://github.com/mumanchu/LTR329ALSOpticalSensor
/*
The LTR-329ALS contains two channels. CH0 measures visible + infrared (IR)
light, and CH1 measures only the infrared. The IR component is subtracted
to produce an accurate LUX reading, from 0.01 to 65535 LUX.

The device has a similar spectral response to the human eye, rejects 50/60Hz
flicker, and contains internal temperature compensation.

This library is smaller and has more features than other LTR392 libraries, 
including automatic gain control (AGC).

LTR-329ALS DATA SHEET
** Page numbers refer to THIS version of the data sheet **
Spec No:        DS86-2014-0006
Effective Date: 09/14/2024
Revision:       E
https://optoelectronics.liteon.com/upload/download/DS86-2014-0006/LTR-329ALS-01_DS_V1.8.PDF

See the muman blog for more details
https://muman.ch/muman/index.htm?muman-light-sensors.htm#muman-ltr329
*/

#include <Wire.h>

/* These may need to be defined
#ifdef DEBUG
#define LOGERROR(s) Serial.println(s); Serial.flush()
#define ASSERT(b) if (!(b)) LOGERROR("ASSERT failed")
#else
#define LOGERROR(s)
#define ASSERT(b)
#endif
*/

class LTR329ALSOpticalSensor
{
protected:
	TwoWire* wire = NULL;
	const int i2cAdds = 0x29;	// fixed I2C address
	byte contrShadow = 0x00;	// ALS_CONTR register shadow
	uint activeGain = -1;		// current settings for automatic gain control
	uint activeTime = -1;
	uint activeRate = -1;
	float gainOverInt = 0.0f;	// pre-calculated in configure() for calculateLux()

public:
	uint agcMin = 500;			// automatic gain control settings
	uint agcMax = 60000;
	uint measurementRateMs = 0;	// time between readings in ms, from measurementRate

	bool begin(TwoWire* twoWire);
	bool reset();
	bool readIDs(uint* partNumber, uint* revision, uint* manufacturer);
	bool setActive(bool active = true);
	bool configure(uint gain, uint integrationTime, uint measurementRate);
	bool readStatus(bool* dataReady, bool* dataInvalid, uint* measurementGain);
	bool readRawData(uint* ch0, uint* ch1);
	float calculateLux(uint ch0, uint ch1);
	bool automaticGainControl(uint ch0, uint ch1);
	uint getActiveGain() { return activeGain; }

protected:
	bool writeRegisters(uint reg, const byte* data, uint length);
	bool readRegisters(uint reg, byte* data, uint length);
	bool readRegister(uint reg, byte* data);
	bool writeRegister(uint reg, byte data);
};


// Reset and configure defaults, device remains in Shutdown mode
// (initialize Wire before calling this, Wire may be shared)
bool LTR329ALSOpticalSensor::begin(TwoWire* twoWire)
{
	wire = twoWire;

	// ensure at least 100ms after power up
	// (millis() returns the no. of milliseconds since the last reset)
	while (millis() <= 100)
		yield();

	// check it's the expected device
	uint partNumber, revision, manufacturer;
	if (!readIDs(&partNumber, &revision, &manufacturer))
		return false;
	if (partNumber != 0x0a || manufacturer != 0x05) {
		LOGERROR("bad ID");
		return false;
	}

	// reset and set the default configuration
	if (!reset())
		return false;
	return configure(0, 0, 3);
}

// Set all registers to default values
// device set to Standby mode
// configure() MUST be called after reset()
bool LTR329ALSOpticalSensor::reset()
{
	// configure() MUST be called after reset
	contrShadow = 0x00;
	activeGain = -1;
	activeTime = -1;
	activeRate = -1;
	gainOverInt = 0.0f;

	bool ok = writeRegister(0x80, 0x02);
	delay(5);		// reset time, this actually takes < 200uS
	// (we could poll bit 1, 0=reset complete, but that's unnecessary code)
	return ok;
}

// Read the hard-wired chip IDs
// partNumber   : 0x0a
// revision     : 0x00 (for my chip)
// manufacturer : 0x05
bool LTR329ALSOpticalSensor::readIDs(uint* partNumber, uint* revision, uint* manufacturer)
{
	byte data[2];
	bool ok = readRegisters(0x86, data, 2);
	*partNumber = data[0] >> 4;
	*revision = data[0] & 0x0f;
	*manufacturer = data[1];
	return ok;
}

// Set Active or Standby mode
// active : true = active, false = standby
// in Standby mode the registers can still be read and written
bool LTR329ALSOpticalSensor::setActive(bool active /*=true*/)
{
	if (active)
		contrShadow |= 0x01;
	else
		contrShadow &= ~0x01;
	bool ok = writeRegister(0x80, contrShadow);
	// 10ms wakeup time from standby
	if (active)
		delay(10);
	return ok;
}

// Configure the gain, integration time and measurement rate
// also pre-calculates a value for the Lux calculations
// the measurementRate must be equal to or greater than the integrationTime (in milliseconds)
// gain            : 0=1x (default), 1=2x, 2=4x, 3=8x, 4=reserved, 5=reserved, 6=48x, 7=96x, p13  
// integrationTime : 0=100ms (default), 1=50ms, 2=200ms, 3=400ms, 4=150ms, 5=250ms, 6=300ms, 7=350ms
// measurementRate : 0=50ms, 1=100ms, 2=200ms, 3=500ms (default), 4=1000ms, 5..7=2000ms
bool LTR329ALSOpticalSensor::configure(uint gain, uint integrationTime, uint measurementRate)
{
	ASSERT(gain < 8 && integrationTime < 8 && measurementRate < 8);
	ASSERT(gain != 4 && gain != 5);

	static const byte time[8] = { 10, 5, 20, 40, 15, 25, 30, 35 };
	static const byte rate[8] = { 5, 10, 20, 50, 100, 200, 200, 200 };

	// the measurementRate must be equal to or greater than the integrationTime
	ASSERT(rate[measurementRate] >= time[integrationTime]);

	// measurementRate in milliseconds, for read timer
	measurementRateMs = rate[measurementRate] * 10;

	// pre-calculate "als_gain / als_int" value for calculateLux(), p20
	static const float alsGain[8] = { 1.0f, 2.0f, 4.0f, 8.0f, 0, 0, 48.0f, 96.0f };
	static const float alsInt[8] = { 1.0f, 0.5f, 2.0f, 4.0f, 1.5f, 2.5f, 3.0f, 3.5f };
	gainOverInt = alsGain[gain] / alsInt[integrationTime];

	contrShadow = (contrShadow & 0x01) | (gain << 2);
	if (!writeRegister(0x80, contrShadow))
		return false;

	if (integrationTime != activeTime || measurementRate != activeRate) {
		if (!writeRegister(0x85, (integrationTime << 3) + measurementRate))
			return false;
		activeTime = integrationTime;
		activeRate = measurementRate;
	}
	activeGain = gain;
	return true;
}

// Read the status register, p21
// use this to poll until dataReady before calling readRawData()
// dataReady       : true when a new reading is available
// dataInvalid     : true if data overflow has occurred, reduce the gain
// measurementGain : the gain value used for the current measurement
bool LTR329ALSOpticalSensor::readStatus(bool* dataReady, bool* dataInvalid, uint* measurementGain)
{
	byte status;
	bool ok = readRegister(0x8c, &status);
	// note: status == 0 if comms fails, so returned values are all 0
	*dataReady = (status & 0x04) != 0;
	*dataInvalid = (status & 0x80) != 0;
	*measurementGain = (status >> 4) & 0x07;
	return ok;
}

// Read the raw data channels CH0 and CH1
// CH0 is visible + IR, CH1 is IR only
bool LTR329ALSOpticalSensor::readRawData(uint* ch0, uint* ch1)
{
	byte data[4];
	if (!readRegisters(0x88, data, 4)) {
		*ch1 = 0;
		*ch0 = 0;
		return false;
	}
	*ch0 = data[2] + (data[3] << 8);	// visible + IR
	*ch1 = data[0] + (data[1] << 8);	// IR only
	return true;
}

// Calculate the LUX value according to the formula on p20
float LTR329ALSOpticalSensor::calculateLux(uint ch0, uint ch1)
{
	// saturation/overflow
	if (ch0 >= 0xffff || ch1 >= 0xffff)
		return 65535.0f;	// 0xffff
	// prevent divide by zero
	if (ch0 == 0.0f && ch1 == 0.0f)
		return 0.0f;

	/* p20
	   RATIO = CH1/(CH0+CH1)
	   IF (RATIO < 0.45)
		   ALS_LUX = (1.7743 * CH0 + 1.1059 * CH1) / ALS_GAIN / ALS_INT
	   ELSEIF (RATIO < 0.64 && RATIO >= 0.45)
		   ALS_LUX = (4.2785 * CH0 – 1.9548 * CH1) / ALS_GAIN / ALS_INT
	   ELSEIF (RATIO < 0.85 && RATIO >= 0.64)
		   ALS_LUX = (0.5926 * CH0 + 0.1185 * CH1) / ALS_GAIN / ALS_INT
	   ELSE
		   ALS_LUX = 0
	   END
	*/
	float lux = 0.0f;
	float ratio = (float)ch1 / (float)(ch0 + ch1);
	if (ratio < 0.45f)
		lux = ((1.7743f * ch0) + (1.1059f * ch1));
	else if (ratio < 0.64f)
		lux = ((4.2785f * ch0) - (1.9548f * ch1));
	else if (ratio < 0.85f)
		lux = (0.5926f * ch0) + (0.1185f * ch1);
	else
		return 0.0f;			// lux = 0
	return lux / gainOverInt;	// gainOverInt is calculated by configure()
}

// Adjust the gain according to the values
// returns true if the gain was changed
// uses agcMin and agcMax, which you can modify if required
bool LTR329ALSOpticalSensor::automaticGainControl(uint ch0, uint ch1)
{
	// gain : 0=1x (default), 1=2x, 2=4x, 3=8x, 4=reserved, 5=reserved, 6=48x, 7=96x, p13  

	// reading too low, increase the gain
	// use only CH0 (visible+IR), CH1 (IR only) can be low, even 0
	if (ch0 < agcMin) {
		if (activeGain == 7)
			return false;		// max gain already
		if (++activeGain == 4)
			activeGain = 6;
		LOGERROR("gain increased");
	}
	// reading too high, decrease the gain
	else if (ch0 > agcMax || ch1 > agcMax) {
		if (activeGain == 0)
			return false;		// min gain already
		if (--activeGain == 5)
			activeGain = 3;
		LOGERROR("gain decreased");
	}
	else
		return false;			// gain not changed

	// update the gain, keep the same integrationTime and measurementRate
	configure(activeGain, activeTime, activeRate);

	return true;				// gain changed
}


// Read/write multiple 8-bit registers

bool LTR329ALSOpticalSensor::readRegisters(uint reg, byte* data, uint length)
{
	memset(data, 0, length);	// return 0s if it fails	

	wire->beginTransmission(i2cAdds);
	if (wire->write((byte)reg) != 1) {
		LOGERROR("write failed");
		return false;
	}
	if (wire->endTransmission() != 0) {
		LOGERROR("endtx failed");
		return false;
	}
	if (wire->requestFrom(i2cAdds, length) != length) {
		//avoid bug in Arduino's Wire.cpp
		//LOGERROR("requestFrom failed");
		//return false;
	}
	if (wire->readBytes(data, length) != length) {
		LOGERROR("readBytes failed");
		return false;
	}
	return true;
}

bool LTR329ALSOpticalSensor::writeRegisters(uint reg, const byte* data, uint length)
{
	wire->beginTransmission(i2cAdds);
	if (wire->write((byte)reg) != 1) {
		LOGERROR("write failed");
		return false;
	}
	if (wire->write(data, length) != length) {
		LOGERROR("write failed");
		return false;
	}
	if (wire->endTransmission() != 0) {
		LOGERROR("endtx failed");
		return false;
	}
	return true;
}


// Read/write a single 8-bit register

bool LTR329ALSOpticalSensor::readRegister(uint reg, byte* data)
{
	return readRegisters(reg, data, 1);
}

bool LTR329ALSOpticalSensor::writeRegister(uint reg, byte data)
{
	return writeRegisters(reg, &data, 1);
}
