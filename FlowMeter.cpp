/*
 * Flow Meter
 */

// Compatibility with the Arduino 1.0 library standard
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include <SD.h>

#include "FlowMeter.h"                                                      // https://github.com/sekdiy/FlowMeter
#include <utils.h>
#include <math.h>

FlowMeter::FlowMeter(unsigned int pin, FlowSensorProperties prop) :
    _pin(pin),                                                              //!< store pin number
    _properties(prop)                                                       //!< store sensor properties
{
  pinMode(pin, INPUT_PULLUP);                                               //!< initialize interrupt pin as input with pullup
}

float_t FlowMeter::getCurrentFlowrate() {
    return this->_currentFlowrate;                                          //!< in l/min
}

float_t FlowMeter::getCurrentVolume() {
    return this->_currentVolume;                                            //!< in l
}

float_t FlowMeter::getTotalFlowrate() {
    return this->_totalVolume / (this->_totalDuration / 1000.0f) * 60.0f;   //!< in l/min
}

float_t FlowMeter::getTotalVolume() {
    return this->_totalVolume;                                              //!< in l
}

void FlowMeter::tick() {
    /* sampling and normalisation */
    unsigned long duration = this->_lastTick;
    float_t seconds = (float_t)duration / 1000.0f;                                    //!< normalised duration (in s, i.e. per 1000ms)
    unsigned long pulses = 0;
    unsigned int interrupts = SREG;                                         //!< save the interrupt status
    cli();                                                                  //!< going to change interrupt variable(s)
    pulses = this->_currentPulses;
    this->_currentPulses = 0;                                               //!< reset pulse counter after successfull sampling
    //!< done changing interrupt variable(s)
    SREG = interrupts;
    float_t frequency = (float_t)pulses / seconds;                      //!< normalised frequency (in 1/s)
    this->_lastTick = 0;

    /* determine current correction factor (from sensor properties) */
    unsigned int decile = floorf(10.0f * frequency / (this->_properties.capacity * this->_properties.kFactor));  //!< decile of current flow relative to sensor capacity
    float_t m_factor = this->_properties.mFactor[min(decile, (unsigned int)9)];
    this->_currentCorrection = this->_properties.kFactor / m_factor;           //!< combine constant k-factor and m-factor for decile

    /* update current calculations: */
    this->_currentFlowrate = frequency / this->_currentCorrection;          //!< get flow rate (in l/min) from normalised frequency and combined correction factor
    this->_currentVolume = this->_currentFlowrate / (60.0f/seconds);        //!< get volume (in l) from normalised flow rate and normalised time

    /* update statistics: */
    this->_currentDuration = duration;                                      //!< store current tick duration (convenience, in ms)
    this->_currentFrequency = frequency;                                    //!< store current pulses per second (convenience, in 1/s)
    this->_totalDuration += duration;                                       //!< accumulate total duration (in ms)
    this->_totalVolume += this->_currentVolume;                             //!< accumulate total volume (in l)
    this->_totalCorrection += this->_currentCorrection * duration;          //!< accumulate corrections over time
    if (frequency > 0.0) {
        Serial.printf("%ld, %04f, %04f, %d, %d, %05f, %04f, %04f, %04f, %04f\r\n", pulses, seconds, frequency, duration, decile, m_factor, _currentCorrection, _currentFlowrate, _currentVolume, _totalVolume);
    }
}

void FlowMeter::count() {
    this->_currentPulses++;                                                 //!< this should be called from an interrupt service routine
}

void FlowMeter::reset() {
    unsigned int interrupts = SREG;                                         //!< save interrupt status
    cli();                                                                  //!< going to change interrupt variable(s)
    this->_currentPulses = 0;                                               //!< reset pulse counter
    SREG = interrupts;                                                      //!< done changing interrupt variable(s)

    this->_currentFrequency = 0.0f;
    this->_lastTick = 0;
    this->_currentDuration = 0.0f;
    this->_currentFlowrate = 0.0f;
    this->_currentVolume = 0.0f;
    this->_currentCorrection = 0.0f;
}

unsigned int FlowMeter::getPin() {
    return this->_pin;
}

unsigned long FlowMeter::getCurrentDuration() {
    return this->_currentDuration;                                          //!< in ms
}

float_t FlowMeter::getCurrentFrequency() {
    return this->_currentFrequency;                                         //!< in 1/s
}

float_t FlowMeter::getCurrentError() {
    /// error (in %) = error * 100
    /// error = correction rate - 1
    /// correction rate = k-factor / correction
    return (this->_properties.kFactor / this->_currentCorrection - 1) * 100;  //!< in %
}

unsigned long FlowMeter::getTotalDuration() {
    return this->_totalDuration;                                            //!< in ms
}

float_t FlowMeter::getTotalError() {
    /// average error (in %) = average error * 100
    /// average error = average correction rate - 1
    /// average correction rate = k-factor / corrections over time * total time
    return (this->_properties.kFactor / this->_totalCorrection * this->_totalDuration - 1) * 100;
}

FlowSensorProperties::FlowSensorProperties(float_t capacity, float_t kFactor) {
	this->capacity = capacity;
	this->kFactor = kFactor;
	for (int i=0; i<10; i++) {
		this->mFactor[i] = 1.0;
	}
}

void FlowSensorProperties::archiveToFile(char *filepath) {
	if (SD.exists(filepath))
		SD.remove(filepath);
	File myFile = SD.open(filepath, FILE_WRITE);
	if (myFile) {
		String str = this->toString();
		myFile.println(str);
#if (SERIAL_DEBUG)
		Serial.printf("%s >> %s\r\n", filepath, str.c_str());
#endif
		myFile.close();
	}
}

void FlowSensorProperties::initializeFromFile(char *filepath) {
	if (SD.exists(filepath)) {
		File myFile = SD.open(filepath);
		String str = myFile.readString();
		this->fromString(str);
#if (SERIAL_DEBUG)
		Serial.printf("%s >> %s\r\n", filepath, str.c_str());
#endif
		myFile.close();
	} else {
		archiveToFile(filepath);
	}
}

void FlowSensorProperties::fromString(String str) {
	if (str.length() <= 0)
		return;
	Split tokens(str, '|');
	this->capacity = tokens.floatToken();
	this->kFactor = tokens.floatToken();
	for (int i=0; i<10; i++) {
		this->mFactor[i] = tokens.floatToken();
	}
}

String FlowSensorProperties::toString() {
	String str;
	str.append(String(this->capacity, 3));
	str.append('|');
	str.append(String(this->kFactor, 3));
	str.append('|');
	for (int i=0; i<10; i++) {
		str.append(String(this->mFactor[i], 5));
		str.append('|');
	}
	return str;
}

FlowSensorProperties UncalibratedSensor(60.0f, 5.0f);
FlowSensorProperties FS300A(60.0f, 5.5f);
FlowSensorProperties FS400A(60.0f, 4.8f);
