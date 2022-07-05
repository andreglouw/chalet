#ifndef LIBRARIES_CHALET_WATERTANKSENSOR_H_
#define LIBRARIES_CHALET_WATERTANKSENSOR_H_

#include <Arduino.h>
#include <FlowMeter.h>
#include <Chalet.h>
#include <Proxies.h>

extern FlowSensorProperties SmallFillSensor;
extern FlowSensorProperties SmallDrainSensor;

enum CALIBRATION_MODE {
	NO_CALIBRATION	= 0,
	DRAIN_MODE 		= 1,
	FILL_MODE		= 2,
};

class WaterFlowSensor {
public:
	WaterFlowSensor();
	virtual ~WaterFlowSensor();
	void initialize(float_t drainedVolume, float_t filledVolume, float_t nettFlow);
	void reset();
	void tick();
	void resetCalibration();
	void setCalibrationMode(CALIBRATION_MODE mode);
	void fillEvent();
	void drainEvent();

	void prepareCalibrationProperties(FlowSensorCalibration *calibrationProps, CALIBRATION_MODE mode);
	void setCalibrationProperties(FlowSensorProperties properties, CALIBRATION_MODE mode);

	void initializeFromFile(char *filepath, FlowSensorProperties *properties);
	void archiveToFile(FlowSensorCalibration *calibration);

	float_t drainedVolume;
	float_t filledVolume;
	float_t calibrateVolume;
	float_t nettFlow;
	bool isDirty;
	CALIBRATION_MODE calibrationMode;
private:
};

extern WaterFlowSensor tankSensor;

typedef struct {
	uint8_t method;
	CALIBRATION_MODE calibrationMode;
	uint8_t decil;
	float_t measuredFlow;
	float_t calculatedFlow;
} __attribute__((__packed__))calibration_t;

class CalibrationProxy : public Proxy {
public:
	~CalibrationProxy();
	Proxy *marshall(void *);
	bool unmarshall(uint8_t *buffer, size_t bufSize);
	size_t bufferSize() { return sizeof(calibration_t); }
	uint8_t *buffer(uint8_t method) {
		dp.method = method;
		return (uint8_t *)&dp;
	}
	calibration_t dp;
};

extern CalibrationProxy calibrationProxy;
extern FlowSensorCalibration calibrationProps;

#endif /* LIBRARIES_CHALET_WATERTANKSENSOR_H_ */
