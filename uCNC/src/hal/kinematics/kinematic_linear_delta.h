/*
	Name: kinematic_linear_delta.h
	Description: Custom kinematics definitions for delta machine

	Copyright: Copyright (c) João Martins
	Author: João Martins
	Date: 06/02/2020

	µCNC is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version. Please see <http://www.gnu.org/licenses/>

	µCNC is distributed WITHOUT ANY WARRANTY;
	Also without the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the	GNU General Public License for more details.
*/

#ifndef KINEMATIC_LINEAR_DELTA_H
#define KINEMATIC_LINEAR_DELTA_H

#ifdef __cplusplus
extern "C"
{
#endif

#if AXIS_COUNT < 3
#error "Linear delta kinematics expects at least 3 axis"
#endif

#ifndef STEPPER0_ANGLE
#define STEPPER0_ANGLE 30
#endif
#ifndef STEPPER1_ANGLE
#define STEPPER1_ANGLE (STEPPER0_ANGLE + 120)
#endif
#ifndef STEPPER2_ANGLE
#define STEPPER2_ANGLE (STEPPER0_ANGLE + 240)
#endif

// the maximum size of the computed segments that are sent to the planner
// this forces linear motions in the delta to treated has an arc motion to
// cope with the non linear kinematic motion of the towers
#ifndef KINEMATICS_MOTION_SEGMENT_SIZE
#define KINEMATICS_MOTION_SEGMENT_SIZE 1.0f
#endif

// minimum arm angle that is allowed for the delta (for software limits)
#ifndef DELTA_ARM_MIN_ANGLE
#define DELTA_ARM_MIN_ANGLE 20
#endif

// maximum angle (should not be bigger then 90º deg angle)
#ifndef DELTA_ARM_MAX_ANGLE
#define DELTA_ARM_MAX_ANGLE 89
#endif

	/*
	Enable Skew compensation
*/
	//#define ENABLE_SKEW_COMPENSATION

#ifdef __cplusplus
}
#endif
#endif
