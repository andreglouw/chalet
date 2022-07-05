/*
 * ShuntSensor.cpp
 *
 *  Created on: 11 Jul 2018
 *      Author: andre.louw
 */

#include <ShuntSensor.h>

ShuntSensor::ShuntSensor(ADS1115 *amplifier, uint8_t gain, uint8_t mux, float_t ampPerMilliVolt)
: current(0.0)
, mVDrop(0.0)
, ampHours(0.0)
, adjustedAmpHours(0.0)
, analogCountRAR(0, false)
, movingAverageCurrent()
, elapsed(0) {
	this->amplifier = amplifier;
	this->gain = gain;
	this->mux = mux;
	this->ampPerMilliVolt = ampPerMilliVolt;
}
bool ShuntSensor::sense(float_t peukertFactor, float_t coulombEfficiency) {
	amplifier->setGain(gain);	// +/- 0.256V
	// Differential conversion (AN0 <-> AN1)
	amplifier->setMultiplexer(mux);
	int16_t adc = amplifier->getConversion();
	float_t perhour = 3600.0/((float_t)elapsed/1000.0);
	float_t sign = (adc >= 0) ? 1.0 : -1.0;
	adc = analogCountRAR.update(abs(adc)).getValue();
	// Count of 2 = ~0.01A. Ignore anything less
	if (adc < 2)
		mVDrop = 0.0;
	else
		mVDrop = adc * amplifier->getMvPerCount() * sign;
	float_t ahFactor = (sign == 1.0) ? coulombEfficiency : peukertFactor;
	current = mVDrop * ampPerMilliVolt;
	movingAverageCurrent.add(current);
	if (current > 0.0) {
		ampHours += (current / perhour);
		adjustedAmpHours += (current * ahFactor / perhour);
	}
	elapsed = 0;
	return true;
}
void ShuntSensor::reset() {
	ampHours = adjustedAmpHours = 0.0;
}
