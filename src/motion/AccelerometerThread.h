#pragma once
#ifndef _ACCELEROMETER_H_
#define _ACCELEROMETER_H_

#include "configuration.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C

#include "../concurrency/OSThread.h"
#include "MotionSensor.h"

// Conditionally include sensor headers only if libraries are available
#ifdef HAS_BMA423
#include "BMA423Sensor.h"
#endif
#if __has_include(<DFRobot_BMM150.h>)
#include "BMM150Sensor.h"
#define HAS_BMM150_LIB 1
#endif
#if __has_include(<Rak_BMX160.h>)
#include "BMX160Sensor.h"
#define HAS_BMX160_LIB 1
#endif
#if __has_include(<ICM_20948.h>)
#include "ICM20948Sensor.h"
#define HAS_ICM20948_LIB 1
#endif
#if __has_include(<Adafruit_LIS3DH.h>)
#include "LIS3DHSensor.h"
#define HAS_LIS3DH_LIB 1
#endif
#if __has_include(<Adafruit_LSM6DS3TRC.h>)
#include "LSM6DS3Sensor.h"
#define HAS_LSM6DS3_LIB 1
#endif
#if __has_include(<Adafruit_MPU6050.h>)
#include "MPU6050Sensor.h"
#define HAS_MPU6050_LIB 1
#endif
#ifdef HAS_QMA6100P
#include "QMA6100PSensor.h"
#endif
#ifdef HAS_STK8XXX
#include "STK8XXXSensor.h"
#endif

extern ScanI2C::DeviceAddress accelerometer_found;

class AccelerometerThread : public concurrency::OSThread
{
  private:
    MotionSensor *sensor = nullptr;
    bool isInitialised = false;

  public:
    explicit AccelerometerThread(ScanI2C::FoundDevice foundDevice) : OSThread("Accelerometer")
    {
        device = foundDevice;
        init();
    }

    explicit AccelerometerThread(ScanI2C::DeviceType type) : AccelerometerThread(ScanI2C::FoundDevice{type, accelerometer_found})
    {
    }

    void start()
    {
        init();
        setIntervalFromNow(0);
    };

    void calibrate(uint16_t forSeconds)
    {
        if (sensor) {
            sensor->calibrate(forSeconds);
        }
    }

  protected:
    int32_t runOnce() override
    {
        // Assume we should not keep the board awake
        canSleep = true;

        if (isInitialised)
            return sensor->runOnce();

        return MOTION_SENSOR_CHECK_INTERVAL_MS;
    }

  private:
    ScanI2C::FoundDevice device;

    void init()
    {
        if (isInitialised)
            return;

        if (device.address.port == ScanI2C::I2CPort::NO_I2C || device.address.address == 0 || device.type == ScanI2C::NONE) {
            LOG_DEBUG("AccelerometerThread Disable due to no sensors found");
            disable();
            return;
        }

        switch (device.type) {
#ifdef HAS_BMA423
        case ScanI2C::DeviceType::BMA423:
            sensor = new BMA423Sensor(device);
            break;
#endif
#ifdef HAS_MPU6050_LIB
        case ScanI2C::DeviceType::MPU6050:
            sensor = new MPU6050Sensor(device);
            break;
#endif
#ifdef HAS_BMX160_LIB
        case ScanI2C::DeviceType::BMX160:
            sensor = new BMX160Sensor(device);
            break;
#endif
#ifdef HAS_LIS3DH_LIB
        case ScanI2C::DeviceType::LIS3DH:
            sensor = new LIS3DHSensor(device);
            break;
#endif
#ifdef HAS_LSM6DS3_LIB
        case ScanI2C::DeviceType::LSM6DS3:
            sensor = new LSM6DS3Sensor(device);
            break;
#endif
#ifdef HAS_STK8XXX
        case ScanI2C::DeviceType::STK8BAXX:
            sensor = new STK8XXXSensor(device);
            break;
#endif
#ifdef HAS_ICM20948_LIB
        case ScanI2C::DeviceType::ICM20948:
            sensor = new ICM20948Sensor(device);
            break;
#endif
#ifdef HAS_BMM150_LIB
        case ScanI2C::DeviceType::BMM150:
            sensor = new BMM150Sensor(device);
            break;
#endif
#ifdef HAS_QMA6100P
        case ScanI2C::DeviceType::QMA6100P:
            sensor = new QMA6100PSensor(device);
            break;
#endif
        default:
            disable();
            return;
        }

        isInitialised = sensor->init();
        if (!isInitialised) {
            clean();
        }
        LOG_DEBUG("AccelerometerThread::init %s", isInitialised ? "ok" : "failed");
    }

    // Copy constructor (not implemented / included to avoid cppcheck warnings)
    AccelerometerThread(const AccelerometerThread &other) : OSThread::OSThread("Accelerometer") { this->copy(other); }

    // Destructor (included to avoid cppcheck warnings)
    virtual ~AccelerometerThread() { clean(); }

    // Copy assignment (not implemented / included to avoid cppcheck warnings)
    AccelerometerThread &operator=(const AccelerometerThread &other)
    {
        this->copy(other);
        return *this;
    }

    // Take a very shallow copy (does not copy OSThread state nor the sensor object)
    // If for some reason this is ever used, make sure to call init() after any copy
    void copy(const AccelerometerThread &other)
    {
        if (this != &other) {
            clean();
            this->device = ScanI2C::FoundDevice(other.device.type,
                                                ScanI2C::DeviceAddress(other.device.address.port, other.device.address.address));
        }
    }

    // Cleanup resources
    void clean()
    {
        isInitialised = false;
        delete sensor;
        sensor = nullptr;
    }
};

#endif

#endif
