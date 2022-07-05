/*
 * BatteryState.cpp
 *
 *  Created on: 01 Jun 2018
 *      Author: andre.louw
 */

#include <Arduino.h>
#include <HardwareSerial.h>
#include <SD.h>
#include <BatteryState.h>
#include <utils.h>

// Maximum time before forcing archive to file
const unsigned long BATTERY_STATE_ARCHIVE_INTERVAL = 600000;
// Change in current that will force archive to file
const float_t CURRENT_ARCHIVE_DELTA = 1.0;

ADS1115 ADCAmplifier1(0x48);
ADS1115 ADCAmplifier2(0x49);

// Resistance of R1, Voltage divider 1, connected to 2nd ADS1115, Analog port 2
const float_t vDivA2R1 = 3298.0;
// R2
const float_t vDiv1R2ADC2A2 = 816.0;
const float_t vCalibrate1 = 1.0;

// Resistance of R1, Voltage divider 2, connected to 2nd ADS1115, Analog port 3
const float_t vDivA3R1 = 3298.0;
// R2
const float_t vDivA3R2 = 817.0;
const float_t vCalibrate2 = 1.0;

elapsedMillis elapsed;

Battery::Battery()
: minBatteryLevel(0.0)
, fullCapacity(1.0)
, capacityHours(20.0)
, peukertValue(1.0)
, coulombEfficiency(1.0) {}

void Battery::fromString(String str) {
	if (str.length() <= 0)
		return;
	Split tokens(str, '|');
	minBatteryLevel = tokens.floatToken();
	fullCapacity = tokens.floatToken();
	capacityHours = tokens.floatToken();
	peukertValue = tokens.floatToken();
	coulombEfficiency = tokens.floatToken();
}

String Battery::toString() {
	String str;
	str.append(String(minBatteryLevel, 5));
	str.append('|');
	str.append(String(fullCapacity, 5));
	str.append('|');
	str.append(String(capacityHours, 5));
	str.append('|');
	str.append(String(peukertValue, 5));
	str.append('|');
	str.append(String(coulombEfficiency, 5));
	str.append('|');
	return str;
}

bool Battery::initializeFromFile() {
	if (SD.exists("battery.txt")) {
		File myFile = SD.open("battery.txt");
		this->fromString(myFile.readString());
		myFile.close();
	} else {
		File myFile = SD.open("batstate.txt", FILE_WRITE);
		if (myFile) {
			myFile.println(this->toString());
			myFile.close();
		}
	}
	return true;
}
void Battery::archiveToFile() {
	if (SD.exists("battery.txt"))
		SD.remove("battery.txt");
	File myFile = SD.open("battery.txt", FILE_WRITE);
	if (myFile) {
		myFile.println(this->toString());
		myFile.close();
	}
}

BatteryState::BatteryState()
: batteryVoltsA2(0.0)
, chaletController(&ADCAmplifier1, ADS1115_PGA_0P256, ADS1115_MUX_P0_N1, (50.0 / 75.0)) // shunt drops 75mV for 50A
, ctekController(&ADCAmplifier1, ADS1115_PGA_0P256, ADS1115_MUX_P2_N3, (100.0 / 75.0))	// shunt drops 75mV for 100A
, adjustedCapacity(0.0)
, movingAverageTimeLeft()
, archivedAmpHours(0.0)
, lastArchive(0)
, peukertFactor(1.0)
, needsArchiving(false) {
}

BatteryState::~BatteryState() {
}

bool BatteryState::init() {
	batteryVoltsA2 = 0.0;
	needsArchiving = false;
	elapsed = 0;
	battery.initializeFromFile();
	// Initialize ADS1115 with defaults
	ADCAmplifier1.initialize();
	if (ADCAmplifier1.testConnection()) {
		ADCAmplifier2.initialize();
		if (ADCAmplifier2.testConnection())
			return true;
		else {
#if (SERIAL_DEBUG)
			Serial.println("Error initializing ADC Amplifier 2");
#endif
		}
	} else {
#if (SERIAL_DEBUG)
		Serial.println("Error initializing ADC Amplifier 1");
#endif
	}
	return false;
}

void BatteryState::monitor() {
	bool stateChange = false;
	if (chaletController.sense(peukertFactor, battery.coulombEfficiency))
		stateChange = needsArchiving = true;
	if (ctekController.sense(peukertFactor, battery.coulombEfficiency))
		stateChange = needsArchiving = true;
	if (stateChange) {
		calculateTimeLeft();
		adjustPeukertFactor();
	}

#if (SERIAL_DEBUG)
	Serial.print(chaletController.mVDrop, 2);
	Serial.print(", ");
	Serial.print(chaletController.current, 3);
	Serial.print(", ");
	Serial.print(chaletController.movingAverageCurrent.average(), 3);
	Serial.print(", ");
	Serial.print(chaletController.ampHours, 3);
	Serial.print(", ");
	Serial.print(ctekController.mVDrop, 3);
	Serial.print(", ");
	Serial.print(ctekController.current, 3);
	Serial.print(", ");
	Serial.print(ctekController.movingAverageCurrent.average(), 3);
	Serial.print(", ");
	Serial.print(ctekController.ampHours, 3);
	Serial.print(", ");
	Serial.print(remainingCapacity(), 2);
	Serial.print(", ");
	Serial.print(batteryVoltsA2, 4);
	Serial.print(", ");
	Serial.print(peukertFactor, 4);
	Serial.print(", ");
	Serial.println(movingAverageTimeLeft.average(), 2);
#endif

	ADCAmplifier2.setGain(ADS1115_PGA_4P096);	// +/- 4.096V
	// Single-ended conversion (AN2 <-> GND)
	ADCAmplifier2.setMultiplexer(ADS1115_MUX_P2_NG);
	float_t vOut = ADCAmplifier2.getMilliVolts();
	batteryVoltsA2 = ((vOut * (vDivA2R1 + vDiv1R2ADC2A2)) / vDiv1R2ADC2A2) / 1000.0 * vCalibrate1;
}

void BatteryState::setFullCapacity(float_t value) {
	battery.fullCapacity = value;
	archiveToFile();
}

void BatteryState::setCoulombEfficiency(float_t value) {
	battery.coulombEfficiency = value;
	battery.archiveToFile();
}

void BatteryState::setPeukertConstant(float_t value) {
	battery.peukertValue = value;
	battery.archiveToFile();
}

/*
 * Batteries are usually specified at an "hour" rate, for instance 100Ahrs at 20 hours. Or 90Ahrs at 10 hours etc.
 * Peukert Capacity (P) = R(C/R)n
 * Then, Battery life = P/In
 * Where:
 * T = time in hours
 * C = the specified capacity of the battery (at the specified hour rating)
 * I = the discharge current
 * n = Battery's Peukert exponent
 * R = the hour rating (ie 20 hours, or 10 hours etc)
 */
void BatteryState::calculateTimeLeft() {
	float_t discharge = nettAverageCurrent();
	adjustedCapacity = pow(remainingCapacity() / battery.capacityHours, battery.peukertValue) * battery.capacityHours;
	if (discharge < 0.0) {
		float_t life = (adjustedCapacity / pow(abs(discharge), battery.peukertValue)) * (battery.minBatteryLevel / 100.0);
		if (life > 500.0)
			life = 500.0;
	    movingAverageTimeLeft.add(life);
	}
}

float_t BatteryState::adjustPeukertFactor() {
	float_t peukAdjusted = battery.fullCapacity;
	float_t discharge = nettAverageCurrent();
	if (discharge < 0.0) {
	    peukAdjusted = battery.fullCapacity * pow(battery.fullCapacity / (abs(discharge) * battery.capacityHours), battery.peukertValue - 1.0);
	}
	peukertFactor = battery.fullCapacity / peukAdjusted;
	if (peukertFactor < 0.1)
		peukertFactor = 0.1;
	else if (peukertFactor > 1.9)
		peukertFactor = 1.9;
	return peukertFactor;
}

void BatteryState::fromString(String str) {
	if (str.length() <= 0)
		return;
	Split tokens(str, '|');
	chaletController.ampHours = tokens.floatToken();
	chaletController.adjustedAmpHours = tokens.floatToken();
	chaletController.movingAverageCurrent.setAverage(tokens.floatToken());
	ctekController.ampHours = tokens.floatToken();
	ctekController.adjustedAmpHours = tokens.floatToken();
	ctekController.movingAverageCurrent.setAverage(tokens.floatToken());
	movingAverageTimeLeft.setAverage(tokens.floatToken());
}

String BatteryState::toString() {
	String str;
	str.append(String(chaletController.ampHours, 5));
	str.append('|');
	str.append(String(chaletController.adjustedAmpHours, 5));
	str.append('|');
	str.append(String(chaletController.averageCurrent(), 5));
	str.append('|');
	str.append(String(ctekController.ampHours, 5));
	str.append('|');
	str.append(String(ctekController.adjustedAmpHours, 5));
	str.append('|');
	str.append(String(ctekController.averageCurrent(), 5));
	str.append('|');
	str.append(String(movingAverageTimeLeft.average(), 5));
	str.append('|');
	return str;
}

bool BatteryState::shouldArchive() {
	return (needsArchiving && (lastArchive >= BATTERY_STATE_ARCHIVE_INTERVAL || abs(archivedAmpHours - nettAmpHours()) >= CURRENT_ARCHIVE_DELTA));
}

void BatteryState::archiveToFile() {
	if (SD.exists("batstate.txt"))
		SD.remove("batstate.txt");
	File myFile = SD.open("batstate.txt", FILE_WRITE);
	if (myFile) {
		myFile.println(this->toString());
		myFile.close();
	}
	archivedAmpHours = nettAmpHours();
	lastArchive = 0;
	needsArchiving = false;
}

bool BatteryState::initializeFromFile() {
	if (init()) {
		if (SD.exists("batstate.txt")) {
			File myFile = SD.open("batstate.txt");
			this->fromString(myFile.readString());
			myFile.close();
		} else {
			File myFile = SD.open("batstate.txt", FILE_WRITE);
			if (myFile) {
				myFile.println(this->toString());
				myFile.close();
			}
		}
		return true;
	}
	return false;
}

void BatteryState::batteryFull() {
	chaletController.reset();
	ctekController.reset();
	needsArchiving = true;
}

BatteryStateProxy::~BatteryStateProxy() {}

Proxy *BatteryStateProxy::marshall(void *object) {
	dp.batteryVoltsA2 = ((BatteryState *)object)->batteryVoltsA2;
	dp.chaletAmpHours = ((BatteryState *)object)->chaletController.ampHours;
	dp.chaletAdjustedAmpHours = ((BatteryState *)object)->chaletController.adjustedAmpHours;
	dp.chaletCurrent = ((BatteryState *)object)->chaletController.current;
	dp.chaletAverageCurrent = ((BatteryState *)object)->chaletController.averageCurrent();
	dp.ctekAmpHours = ((BatteryState *)object)->ctekController.ampHours;
	dp.ctekAdjustedAmpHours = ((BatteryState *)object)->ctekController.adjustedAmpHours;
	dp.ctekCurrent = ((BatteryState *)object)->ctekController.current;
	dp.ctekAverageCurrent = ((BatteryState *)object)->ctekController.averageCurrent();
	dp.timeLeft = ((BatteryState *)object)->movingAverageTimeLeft.average();
	dp.fullCapacity = ((BatteryState *)object)->battery.fullCapacity;
	dp.adjustedCapacity = ((BatteryState *)object)->adjustedCapacity;
	dp.remainingCapacity = ((BatteryState *)object)->remainingCapacity();
	dp.peukertFactor = ((BatteryState *)object)->peukertFactor;
	dp.coulombEfficiency = ((BatteryState *)object)->battery.coulombEfficiency;
	dp.batteryPeukert = ((BatteryState *)object)->battery.peukertValue;
	return this;
}

bool BatteryStateProxy::unmarshall(uint8_t *buffer, size_t bufSize) {
	memcpy(&dp, buffer, bufSize);
	return true;
}

BatteryState batteryState;
BatteryStateProxy batteryStateProxy;
