/**
 * @file MotorControllerFactory.h
 * @brief Creates the correct IMotorController based on compile-time role flag
 *
 * Usage in main.cpp:
 *   IMotorController* motor = MotorControllerFactory::create();
 *   motor->begin(config);
 *
 * @author FluidLevelMonitor Project
 * @version 2.0.0
 */
#ifndef MOTOR_CONTROLLER_FACTORY_H
#define MOTOR_CONTROLLER_FACTORY_H

#include "IMotorController.h"

#if defined(FLM_ROLE_MOTOR_RELAY)
  #include "RelayMotorController.h"
#elif defined(FLM_ROLE_MOTOR_SMS)
  #include "SmsMotorController.h"
#endif

class MotorControllerFactory {
public:
    /**
     * @brief Create the motor controller instance selected at compile time.
     * Returns nullptr if compiled as sensor_only role.
     * Caller owns the pointer.
     */
    static IMotorController* create() {
#if defined(FLM_ROLE_MOTOR_RELAY)
        return new RelayMotorController();
#elif defined(FLM_ROLE_MOTOR_SMS)
        return new SmsMotorController();
#else
        return nullptr;  // sensor_only role — no motor control
#endif
    }
};

#endif // MOTOR_CONTROLLER_FACTORY_H
