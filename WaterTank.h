#ifndef LIBRARIES_CHALET_WATERTANK_H_
#define LIBRARIES_CHALET_WATERTANK_H_

#include <Arduino.h>
#include <Chalet.h>
#include <Proxies.h>

class WaterFlowSensor;

extern float_t TANK_CAPACITY;

class WaterTank {
public:
	WaterTank(float_t volume);
	virtual ~WaterTank();

	void init(float_t volume);
	void fromString(String str);
	String toString();

	void tankFilled(float_t volume);
	void tankEmpty();
	float_t remainingLitres() { return  tankVolume - drainedVolume + filledVolume; }

	void initializeFromFile();
	bool shouldArchive();
	void archiveToFile();

	void updateFromSensor(WaterFlowSensor sensor);

	bool needsArchiving;
	bool wasUpdated;
	elapsedMillis lastFlow;
	float_t tankVolume;
	float_t drainedVolume;
	float_t filledVolume;
	float_t nettFlow;
	float_t calibrateVolume;
	float_t measuredVolume;
};

typedef struct {
	uint8_t method;
	float_t tankVolume;
	float_t drainedVolume;
	float_t filledVolume;
	float_t nettFlow;
	float_t calibrateVolume;
	float_t measuredVolume;
	float_t remainingLitres;
} __attribute__((__packed__))watertank_t;

class WaterTankProxy : public Proxy {
public:
	~WaterTankProxy();
	Proxy *marshall(void *);
	bool unmarshall(uint8_t *buffer, size_t bufSize);
	size_t bufferSize() { return sizeof(watertank_t); }
	uint8_t *buffer(uint8_t method) {
		dp.method = method;
		return (uint8_t *)&dp;
	}
	watertank_t dp;
};

extern WaterTank tank;
extern WaterTankProxy waterTankProxy;

#endif /* LIBRARIES_CHALET_WATERTANK_H_ */
