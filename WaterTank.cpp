#include <Arduino.h>
#include <SD.h>
#include <WaterTank.h>
#include <utils.h>

float_t TANK_CAPACITY	= 60.0;

// How long before status must be archived after flow has stopped
const unsigned long NO_FLOW_PROPERTY_UPDATE_TIME = 600000;

WaterTank::WaterTank(float_t volume)
: tankVolume(volume)
, drainedVolume(0.0)
, filledVolume(0.0)
, nettFlow(0.0)
, calibrateVolume(0.0)
, measuredVolume(0.0)
, needsArchiving(false)
, wasUpdated(false)
, lastFlow(0) {
}

void WaterTank::fromString(String str) {
	if (str.length() <= 0)
		return;
	Split tokens(str, '|');
	tankVolume = tokens.floatToken();
	drainedVolume = tokens.floatToken();
	filledVolume = tokens.floatToken();
	nettFlow = tokens.floatToken();
	calibrateVolume = tokens.floatToken();
	wasUpdated = true;
	lastFlow = 0;
}

void WaterTank::init(float_t volume) {
	tankVolume = volume;
	drainedVolume = 0.0;
	filledVolume = 0.0;
	nettFlow = 0.0;
	calibrateVolume = 0.0;
	measuredVolume = 0.0;
	needsArchiving = false;
	wasUpdated = false;
	lastFlow = 0;
}

String WaterTank::toString() {
	String str;
	str.append(tankVolume);
	str.append('|');
	str.append(String(drainedVolume, 5));
	str.append('|');
	str.append(String(filledVolume, 5));
	str.append('|');
	str.append(String(nettFlow, 5));
	str.append('|');
	str.append(String(calibrateVolume, 5));
	str.append('|');
	return str;
}

bool WaterTank::shouldArchive() {
	return (needsArchiving && lastFlow > NO_FLOW_PROPERTY_UPDATE_TIME);
}

void WaterTank::archiveToFile() {
	if (SD.exists("tank.txt"))
		SD.remove("tank.txt");
	File myFile = SD.open("tank.txt", FILE_WRITE);
	if (myFile) {
		myFile.println(this->toString());
		myFile.close();
	}
	nettFlow = 0.0;
	needsArchiving = false;
	lastFlow = 0;
}

void WaterTank::initializeFromFile() {
	init(TANK_CAPACITY);
	if (SD.exists("tank.txt")) {
		File myFile = SD.open("tank.txt");
		this->fromString(myFile.readString());
		myFile.close();
	} else {
		File myFile = SD.open("tank.txt", FILE_WRITE);
		if (myFile) {
			myFile.println(this->toString());
			myFile.close();
		}
		wasUpdated = true;
	}
}

void WaterTank::updateFromSensor(WaterFlowSensor sensor) {
	drainedVolume = sensor.drainedVolume;
	filledVolume = sensor.filledVolume;
	nettFlow = sensor.nettFlow;
	calibrateVolume = sensor.calibrateVolume;
	needsArchiving = true;
	wasUpdated = true;
	lastFlow = 0;
}

void WaterTank::tankFilled(float_t volume) {
	tankVolume = volume;
	drainedVolume = 0.0;
	filledVolume = 0.0;
	nettFlow = 0.0;
	needsArchiving = true;
	wasUpdated = true;
}

void WaterTank::tankEmpty() {
	tankVolume = 0.0;
	drainedVolume = 0.0;
	filledVolume = 0.0;
	nettFlow = 0.0;
	needsArchiving = true;
	wasUpdated = true;
}

WaterTank::~WaterTank() {}

WaterTankProxy::~WaterTankProxy() {}

Proxy *WaterTankProxy::marshall(void *object) {
	dp.tankVolume = ((WaterTank *)object)->tankVolume;
	dp.drainedVolume = ((WaterTank *)object)->drainedVolume;
	dp.filledVolume = ((WaterTank *)object)->filledVolume;
	dp.nettFlow = ((WaterTank *)object)->nettFlow;
	dp.calibrateVolume = ((WaterTank *)object)->calibrateVolume;
	dp.remainingLitres = ((WaterTank *)object)->remainingLitres();
	return this;
}

bool WaterTankProxy::unmarshall(uint8_t *buffer, size_t bufSize) {
	memcpy(&dp, buffer, bufSize);
	return true;
}

CalibrationProxy::~CalibrationProxy() {}

Proxy *CalibrationProxy::marshall(void *object) {
	return this;
}

bool CalibrationProxy::unmarshall(uint8_t *buffer, size_t bufSize) {
	memcpy(&dp, buffer, bufSize);
	return true;
}

WaterTank tank(TANK_CAPACITY);
CalibrationProxy calibrationProxy;
WaterTankProxy waterTankProxy;
