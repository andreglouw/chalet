#include <Arduino.h>
#include <SD.h>
#include <WaterFlowSensor.h>

void FillMeterISR();
void DrainMeterISR();

// Fill sensor
FlowSensorProperties SmallFillSensor(30.0f, 4.5f);
// Drain sensor
FlowSensorProperties SmallDrainSensor(30.0f, 2.2f);

FlowSensorCalibration calibrationProps(SmallFillSensor);

FlowMeter fillMeter = FlowMeter(DI_FILLFLOW_PIN, SmallFillSensor);
FlowMeter drainMeter = FlowMeter(DI_DRAINFLOW_PIN, SmallDrainSensor);

WaterFlowSensor::WaterFlowSensor() {
	drainedVolume = 0.0;
	filledVolume = 0.0;
	nettFlow = 0.0;
	isDirty = false;
	calibrateVolume = 0.0;
	calibrationMode = NO_CALIBRATION;
}

WaterFlowSensor::~WaterFlowSensor() {
}

void WaterFlowSensor::initialize(float_t drainedVolume, float_t filledVolume, float_t nettFlow) {
	SmallFillSensor.initializeFromFile("fill.txt");
	fillMeter.setProperties(SmallFillSensor);
	SmallDrainSensor.initializeFromFile("drain.txt");
	drainMeter.setProperties(SmallDrainSensor);
    // enable a call to the 'interrupt service handler' (ISR) on every rising edge at the FillMeter interrupt pin
	attachInterrupt(digitalPinToInterrupt(DI_FILLFLOW_PIN), FillMeterISR, RISING);
    // enable a call to the 'interrupt service handler' (ISR) on every rising edge at the DrainMeter interrupt pin
	attachInterrupt(digitalPinToInterrupt(DI_DRAINFLOW_PIN), DrainMeterISR, RISING);
	this->reset();
	this->filledVolume = filledVolume;
	this->drainedVolume = drainedVolume;
	this->nettFlow = nettFlow;
	this->calibrateVolume = 0.0;
}

void WaterFlowSensor::reset() {
	filledVolume = 0.0;
	fillMeter.reset();
	drainedVolume = 0.0;
	drainMeter.reset();
	nettFlow = 0.0;
	calibrateVolume = 0.0;
}

void WaterFlowSensor::resetCalibration() {
	this->calibrationMode = NO_CALIBRATION;
	calibrateVolume = 0.0;
}

void WaterFlowSensor::setCalibrationMode(CALIBRATION_MODE mode) {
	this->calibrationMode = mode;
	switch (mode) {
		case DRAIN_MODE:
			drainMeter.reset();
			calibrateVolume = 0.0;
			break;
		case FILL_MODE:
			fillMeter.reset();
			calibrateVolume = 0.0;
			break;
		default:
			break;
	}
}

void WaterFlowSensor::tick() {
    fillMeter.tick();
    drainMeter.tick();
	drainedVolume += drainMeter.getCurrentVolume();
	filledVolume += fillMeter.getCurrentVolume();
	switch (calibrationMode) {
		case DRAIN_MODE:
			calibrateVolume += drainMeter.getCurrentVolume();
			break;
		case FILL_MODE:
			calibrateVolume += fillMeter.getCurrentVolume();
			break;
		default:
			break;
	}
	nettFlow = fillMeter.getCurrentFlowrate() - drainMeter.getCurrentFlowrate();
	if (nettFlow != 0.0) {
		isDirty = true;
	}
}
void WaterFlowSensor::fillEvent() {
	fillMeter.count();
}
void WaterFlowSensor::drainEvent() {
	drainMeter.count();
}

void WaterFlowSensor::prepareCalibrationProperties(FlowSensorCalibration *calibrationProps, CALIBRATION_MODE mode) {
	switch (mode) {
		case DRAIN_MODE:
			calibrationProps->setProperties(drainMeter.getProperties());
			break;
		case FILL_MODE:
			calibrationProps->setProperties(fillMeter.getProperties());
			break;
		default:
			break;
	}
}
void WaterFlowSensor::setCalibrationProperties(FlowSensorProperties properties, CALIBRATION_MODE mode) {
	switch (mode) {
		case DRAIN_MODE:
			drainMeter.setProperties(properties);
			properties.archiveToFile("drain.txt");
			break;
		case FILL_MODE:
			fillMeter.setProperties(properties);
			properties.archiveToFile("fill.txt");
			break;
		default:
			break;
	}
}

WaterFlowSensor tankSensor;

void FillMeterISR() {
	tankSensor.fillEvent();
}

void DrainMeterISR() {
	tankSensor.drainEvent();
}
