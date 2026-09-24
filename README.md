# LTR329ALSOpticalSensor

**Arduino Library for the LTR-329ALS-01 Optical Sensor**

If you need accurate LUX readings, this chip is recommended. Unlike the complicated LTR-507, it's very easy to use and I did not find any problems. The LUX readings compare perfectly with my LUX meter.
The LTR-329ALS contains two channels. CH0 measures visible + infrared (IR) light, and CH1 measures only the infrared. The IR component is subtracted to produce an accurate LUX reading, from 0.01 to 65535 LUX. The device has a similar spectral response to the human eye, rejects 50/60Hz flicker, and contains internal temperature compensation. In short, it's really good!
For testing, I used an Adafruit QT breakout board. These are not easy to find, but they are still available on Digikey (CHF3.61). You can buy the tiny SMD chips too for around 50cts, but you'll need to use solder paste and a hot plate to solder them safely.

<img width="300" height="215" alt="image" src="https://github.com/user-attachments/assets/e91dc251-b638-498e-a189-29a45757c72e" />

The Adafruit library is not so good, so this new library was developed, which has additional features like "automatic gain control", and it does not use the horrible `malloc()` and `free()` all the time.

## Installation

The library can be installed in the Arduino IDE by downloading the ZIP file, and using 'Sketch / Include Library > Add ZIP Library...' This installs the library and the example sketch. Open the example sketch with 'File / Examples / Example from custom libraries'.


## Class Reference

For full details, refer to the comments in the source code.

The example sketch uses `sprintf(buf, "%f", ...)` for floats. If your setup does not support this, use the `floatToString()` function which you can find here: https://github.com/mumanchu/mumanchu/blob/main/utils/Float.cpp

```cpp
class LTR329ALSOpticalSensor
{
public:
	uint agcMin = 500;			  // automatic gain control settings
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
};
```


## Revision History

| Date  | Revision | Description |
|:---------- |:---------|:----------- |
| 2026.09.24 | 0.0.0	| Preliminary |

<br/>

## Joke of the Week

I wanted a _nap_, not an _app_, you fule!
