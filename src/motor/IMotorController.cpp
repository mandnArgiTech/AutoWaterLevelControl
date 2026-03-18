#include "IMotorController.h"

String IMotorController::modeToString(MotorMode m) {
    switch (m) {
        case MotorMode::OFF:  return "off";
        case MotorMode::AUTO: return "auto";
        case MotorMode::ON:   return "on";
        default:              return "unknown";
    }
}

String IMotorController::stateToString(MotorState s) {
    switch (s) {
        case MotorState::STOPPED:  return "stopped";
        case MotorState::RUNNING:  return "running";
        case MotorState::DISABLED: return "disabled";
        case MotorState::PENDING:  return "pending";
        default:                   return "unknown";
    }
}

MotorMode IMotorController::modeFromString(const String& s) {
    if (s == "auto") return MotorMode::AUTO;
    if (s == "on")   return MotorMode::ON;
    return MotorMode::OFF;
}
