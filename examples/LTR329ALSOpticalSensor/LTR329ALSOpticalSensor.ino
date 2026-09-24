///////////////////////////////////////////////////////////////////////////////
// Example sketch for the LTR329ALSOptical Sensor library
// Copyright (C) muman.ch + github.com/mumanchu, 2026.09.24
// If you re-use this code, please include this copyright notice
// info@muman.ch
// See github for details,
// https://github.com/mumanchu/LTR329ALSOpticalSensor

#include <Wire.h>

typedef unsigned int uint;
typedef unsigned long ulong;

#define LOGERROR(s) { Serial.println(s); Serial.flush(); }
#define ASSERT(b) if (!(b)) { LOGERROR("assert failed"); return false; }

#include "LTR329ALSOpticalSensor.h"
LTR329ALSOpticalSensor ltr329;


void setup()
{
	// use different pins for RX/TX logging
	// this is only for the STM32 Nucleo-64 boards
	#ifdef ARDUINO_NUCLEO_64
	Serial.setTx(PC_10);
	Serial.setRx(PC_11);
	#endif

	Serial.begin(115200);
	delay(3000);

	// PuTTY clear screen and scrollback
	// patch out if not using PuTTY
	Serial.print("\033[2J\033[H\033[3J");

	Serial.println("\n\rStarted\n\r");
	Serial.flush();

	// XIAO has no LED
	#ifdef LED_BUILTIN
	pinMode(LED_BUILTIN, OUTPUT);
	#endif

	Wire.begin();
	Wire.setClock(400000);

	if (!ltr329.begin(&Wire)) {
		Serial.println("ltr329.begin() failed");
		Serial.flush();
		while (1)
			yield();
	}
	ltr329.setActive(true);
}


void loop()
{
	char buf[256];

	ulong t = millis();

	static ulong t1 = 0;
	if (t - t1 >= 100) {
		t1 = t;

		#ifdef LED_BUILTIN
		digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
		#endif

		bool dataReady;
		bool dataInvalid;
		uint measurementGain;
		ltr329.readStatus(&dataReady, &dataInvalid, &measurementGain);

		if (dataReady) {
			uint ch0, ch1;
			if (ltr329.readRawData(&ch0, &ch1)) {
				float lux = ltr329.calculateLux(ch0, ch1);

				sprintf(buf, "lux=%f  ch0=%u  ch1=%u  dataValid=%s  gain=%u",
					lux, ch0, ch1, dataInvalid ? "false" : "true", measurementGain);
				Serial.println(buf);
				Serial.flush();

				bool gainChanged = ltr329.automaticGainControl(ch0, ch1);
			}
		}
	}
}
