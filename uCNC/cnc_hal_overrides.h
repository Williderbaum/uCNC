#ifndef CNC_HAL_OVERRIDES_H
#define CNC_HAL_OVERRIDES_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "cnc_hal_reset.h"
#define ENABLE_COOLANT
#define S_CURVE_ACCELERATION_LEVEL 0
#define ESTOP_PULLUP_ENABLE
#define LIMIT_X_PULLUP_ENABLE
#define LIMIT_Y_PULLUP_ENABLE
#define LIMIT_X2_PULLUP_ENABLE
#define LIMIT_Y2_PULLUP_ENABLE
#define DISABLE_PROBE
#define TOOL1 laser_pwm
#define LASER_PWM PWM0
#define LASER_FREQ 8000
#define LASER_PWM_AIR_ASSIST DOUT2
#define ENCODERS 0
//Custom configurations
// disable some features to make the code smaller
#define DISABLE_SETTINGS_MODULES
#define DISABLE_MULTISTREAM_SERIAL
#define DISABLE_RTC_CODE

#ifdef __cplusplus
}
#endif
#endif
