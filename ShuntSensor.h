/*
 * ShuntSensor.h
 *
 *  Created on: 11 Jul 2018
 *      Author: andre.louw
 */

#ifndef LIBRARIES_CHALET_SHUNTSENSOR_H_
#define LIBRARIES_CHALET_SHUNTSENSOR_H_

#include <Arduino.h>
#include <ADS1115.h>
#include <ResponsiveAnalogRead.h>
#include <utils.h>

class ShuntSensor {
public:
	ShuntSensor(ADS1115 *amplifier, uint8_t gain, uint8_t mux, float_t ampPerMilliVolt);
	bool sense(float_t peukertFactor, float_t coulombEfficiency);
	float_t averageCurrent() { return movingAverageCurrent.average(); }
	void reset();

	ADS1115 *amplifier;
	uint8_t gain;
	uint8_t mux;
	float_t ampPerMilliVolt;
	float_t current;
	float_t mVDrop;
	float_t ampHours;
	float_t adjustedAmpHours;
	ResponsiveAnalogRead analogCountRAR;
	MovingAvgLastN movingAverageCurrent;
	elapsedMillis elapsed;
};

#endif /* LIBRARIES_CHALET_SHUNTSENSOR_H_ */
