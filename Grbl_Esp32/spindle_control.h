/*
  spindle.h - Header for system level commands and real-time processes
  Part of Grbl
  Copyright (c) 2014-2016 Sungeun K. Jeon for Gnea Research LLC
	
	2018 -	Bart Dring This file was modified for use on the ESP32
					CPU. Do not use this with Grbl for atMega328P
	
  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef spindle_control_h
  #define spindle_control_h
  
#include "grbl.h"
  
#define SPINDLE_PWM_RANGE         (SPINDLE_PWM_MAX_VALUE-SPINDLE_PWM_MIN_VALUE)

#define SPINDLE_NO_SYNC false
#define SPINDLE_FORCE_SYNC true

#define SPINDLE_STATE_DISABLE  0  // Must be zero.
#define SPINDLE_STATE_CW       bit(0)
#define SPINDLE_STATE_CCW      bit(1)

#define SPINDLE_PID_TIMER 3
#define SPINDLE_PULSE_UNIT PCNT_UNIT_0
#define SPINDLE_PID_UPDATE_PERIOD 0.2 // in s
#define SPINDLE_PID_KP  5
#define SPINDLE_PID_KI  0
#define SPINDLE_PID_KD  0
  
  void spindle_init();
  void spindle_stop();
  uint8_t spindle_get_state();
  void spindle_set_speed(uint32_t pwm_value);
  void spindle_pid_task(void *pvParameters);
  uint32_t spindle_compute_pwm_value(float rpm);
  void spindle_set_state(uint8_t state, float rpm);
  void spindle_sync(uint8_t state, float rpm);
  void grbl_analogWrite(uint8_t chan, uint32_t duty);
  void spindle_set_enable(bool enable);
  uint32_t piecewise_linear_fit(float rpm);

#endif
