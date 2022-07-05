/*
 * BatteryState.h
 *
 *  Created on: 01 Jun 2018
 *      Author: andre.louw
 */

#ifndef LIBRARIES_CHALET_BATTERYSTATE_H_
#define LIBRARIES_CHALET_BATTERYSTATE_H_

#include <ShuntSensor.h>
#include <Proxies.h>

/*
 * 2 x Deltec 657 12V 74AH/20HR batteries
 * 		20 Hour Rate - Will provide (74/20) = 3.7 Amps for 20 Hours
 * 130MIN/RC:
 * 		Reserve Capacity - number of minutes a fully charged battery at 26.7 degrees C is discharged at 25 amps before the voltage falls below 10.5 volts.
 * 		To convert Reserve Capacity (RC) to Ampere-Hours at the 25 amp rate, multiple RC by .4167. More ampere-hours (or RC) are better in every case.
 * 680A/CCA:
 * 		Cold Cranking Amps - number of amperes a lead acid battery at 0 degrees F can deliver for 30 seconds and maintain at least 1.2 volts per cell
*/

// Peukert value calculated using Resources/Peukert_calculations.xls
// Coulomb efficiency is thumbsuck...

/*
 * Create battery.txt:
75.0|148.0|20.0|1.16|0.9|
 */
class Battery {
public:
	Battery();
	void fromString(String str);
	String toString();
	bool initializeFromFile();
	bool shouldArchive();
	void archiveToFile();

	float minBatteryLevel;
	float fullCapacity;
	float capacityHours;
	float peukertValue;
	float coulombEfficiency;
};

class BatteryState {
public:
	BatteryState();
	virtual ~BatteryState();

	bool init();
	void monitor();

	void setFullCapacity(float_t full);
	void setCoulombEfficiency(float_t full);
	void setPeukertConstant(float_t full);

	void calculateTimeLeft();
	float_t adjustPeukertFactor();

	float_t remainingCapacity() {
		float_t result = battery.fullCapacity+nettAmpHours();
		if (result > battery.fullCapacity)
			result = battery.fullCapacity;
		if (result < 0.0)
			result = 0.0;
		return result;
	}
	float_t nettAmpHours() {
		return chaletController.ampHours+ctekController.ampHours;
	}

	float_t nettAdjustedAmpHours() {
		return chaletController.adjustedAmpHours+ctekController.adjustedAmpHours;
	}

	float_t nettCurrent() {
		return chaletController.current+ctekController.current;
	}

	float_t nettAverageCurrent() {
		return chaletController.averageCurrent()+ctekController.averageCurrent();
	}

	void fromString(String str);
	String toString();

	bool initializeFromFile();
	bool shouldArchive();
	void archiveToFile();
	void batteryFull();

	ShuntSensor chaletController;
	ShuntSensor ctekController;
	Battery battery;
	float_t batteryVoltsA2;
	float_t adjustedCapacity;
	MovingAvgLastN movingAverageTimeLeft;
	float_t archivedAmpHours;
	elapsedMillis lastArchive;
	float_t peukertFactor;
	bool needsArchiving;
};

typedef struct {
	uint8_t method;
	float_t batteryVoltsA2;
	float_t chaletAmpHours;
	float_t chaletAdjustedAmpHours;
	float_t chaletCurrent;
	float_t chaletAverageCurrent;
	float_t ctekAmpHours;
	float_t ctekAdjustedAmpHours;
	float_t ctekCurrent;
	float_t ctekAverageCurrent;
	float_t timeLeft;
	float_t fullCapacity;
	float_t adjustedCapacity;
	float_t remainingCapacity;
	float_t peukertFactor;
	float_t coulombEfficiency;
	float_t batteryPeukert;
} __attribute__((__packed__))battery_state_t;

class BatteryStateProxy : public Proxy {
public:
	~BatteryStateProxy();
	Proxy *marshall(void *);
	bool unmarshall(uint8_t *buffer, size_t bufSize);
	size_t bufferSize() { return sizeof(battery_state_t); }
	uint8_t *buffer(uint8_t method) {
		dp.method = method;
		return (uint8_t *)&dp;
	}
	battery_state_t dp;
};

extern BatteryState batteryState;
extern BatteryStateProxy batteryStateProxy;

#endif /* LIBRARIES_CHALET_BATTERYSTATE_H_ */
