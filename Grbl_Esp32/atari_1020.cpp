/*
  atari_1020.cpp
  Part of Grbl_ESP32
      
	copyright (c) 2018 -	Bart Dring This file was modified for use on the ESP32
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
  
  --------------------------------------------------------------
  
  The solenoid attracts or replels a magnet based on the direction
  of the current in the coil. Pen up is one direction and pen down 
  is the other direction.
  
  The coil is always on. To prevent over heating, the current is lowered
  after a few milliseconds. It starts in a 'pull' strength, then is lowered
  to a 'hold' strength.
  
 
  
  
	
*/
#include "grbl.h"

#ifdef ATARI_1020

static TaskHandle_t solenoidSyncTaskHandle = 0;
static TaskHandle_t atariHomingTaskHandle = 0;

uint16_t solenoid_pull_count;

void machine_init()
{				
	solenoid_pull_count = 0; // initialize
	
	grbl_send(CLIENT_SERIAL, "[MSG:Atari 1020 Solenoid]\r\n");
	
	// setup PWM channel
	ledcSetup(SOLENOID_CHANNEL_NUM, SOLENOID_PWM_FREQ, SOLENOID_PWM_RES_BITS);
	ledcAttachPin(SOLENOID_PEN_PIN, SOLENOID_CHANNEL_NUM);
		
	pinMode(SOLENOID_DIRECTION_PIN, OUTPUT);  // this sets the direction of the solenoid current	
	pinMode(X_LIMIT_PIN, INPUT); // external pullup required
	
	// setup a task that will calculate the determine and set the servo position		
	xTaskCreatePinnedToCore(	solenoidSyncTask,    // task
								"solenoidSyncTask", // name for task
								4096,   // size of task stack
								NULL,   // parameters
								1, // priority
								&solenoidSyncTaskHandle,
								0 // core
							);													
}

// this task tracks the Z position and sets the solenoid
void solenoidSyncTask(void *pvParameters)
{		
	int32_t current_position[N_AXIS]; // copy of current location
	float m_pos[N_AXIS];   // machine position in mm
	TickType_t xLastWakeTime;
	const TickType_t xSolenoidFrequency = SOLENOID_TASK_FREQ;  // in ticks (typically ms)

	xLastWakeTime = xTaskGetTickCount(); // Initialise the xLastWakeTime variable with the current time.
	while(true) { // don't ever return from this or the task dies
							
		memcpy(current_position,sys_position,sizeof(sys_position));  // get current position in step	
		system_convert_array_steps_to_mpos(m_pos,current_position); // convert to millimeters				
		calc_solenoid(m_pos[Z_AXIS]); // calculate kinematics and move the servos
						
		vTaskDelayUntil(&xLastWakeTime, xSolenoidFrequency);
    }
}

void atari_home() {
	// create and start a task to do the special homing
	grbl_send(CLIENT_SERIAL, "[MSG:Atari begin homing]\r\n");
	
	// setup a task that will calculate the determine and set the servo position		
	xTaskCreatePinnedToCore(	atari_home_task,    // task
								"atari_home_task", // name for task
								4096,   // size of task stack
								NULL,   // parameters
								1, // priority
								&atariHomingTaskHandle,
								0 // core
							);	
}

void atari_home_task(void *pvParameters) {
	#define HOMING_PHASE_FULL_APPROACH	0 // move to right end
	#define HOMING_PHASE_CHECK			1 // check reed switch
	#define HOMING_PHASE_RETRACT 		2 // retract
	#define HOMING_PHASE_SHORT_APPROACH	3 // retract
	
	//bool homing = true;
	uint8_t homing_attemp_num = 0; // how many times have we tried to home
	TickType_t xLastWakeTime;
	const TickType_t xHomingTaskFrequency = 200;  // in ticks (typically ms) .... need to make sure there is enough time to get out of idle
	uint8_t homing_phase = HOMING_PHASE_FULL_APPROACH;
	
	while(true) { // this task will only last as long as it is homing
		// must be in idle or alarm state
		switch(homing_phase) {
			case HOMING_PHASE_FULL_APPROACH:
				grbl_send(CLIENT_SERIAL, "[MSG: Atari homing full]\r\n");
				inputBuffer.push("G0X-100\r"); // run SD card file 2.nc
				homing_attemp_num = 1;
				homing_phase = HOMING_PHASE_CHECK;
			break;
			case HOMING_PHASE_CHECK:
				grbl_send(CLIENT_SERIAL, "[MSG: Atari homing check]\r\n");
				if (digitalRead(X_LIMIT_PIN) == 0) { // reed switch closes to ground
					grbl_send(CLIENT_SERIAL, "[MSG: Atari homing success]\r\n");
					
					// TO DO 
					// Set the machine position				
					// Clear the alarm
					// move to X0 ... paper 0
					return;  // next iteration will break from loop
				}
				homing_attemp_num++;
			break;
			case HOMING_PHASE_RETRACT:
				grbl_send(CLIENT_SERIAL, "[MSG: Atari homing retract]\r\n");
				inputBuffer.push("G0X10\r"); // run SD card file 2.nc
				homing_phase = HOMING_PHASE_SHORT_APPROACH;
			break;
			case HOMING_PHASE_SHORT_APPROACH:
				grbl_send(CLIENT_SERIAL, "[MSG: Atari homing short]\r\n");
				inputBuffer.push("G0X-10\r"); // run SD card file 2.nc
				homing_phase = HOMING_PHASE_CHECK;
			break;
			default:
				grbl_sendf(CLIENT_SERIAL, "[MSG:Homing phase error %d]\r\n", homing_phase);
				return; // kills task
			break;
		}	
	
		if (homing_attemp_num > 12) { // there are only 12 positions to try
			grbl_send(CLIENT_SERIAL, "[MSG: Atari homing failed]\r\n");
			return;
		}
		vTaskDelayUntil(&xLastWakeTime, xHomingTaskFrequency);
	}	
}


// calculate and set the PWM value for the servo
void calc_solenoid(float penZ)
{	
	bool isPenUp;
	static bool previousPenState = false;
	uint32_t solenoid_pen_pulse_len;  // duty cycle of solenoid		
		
	isPenUp = ( (penZ > 0) || (sys.state == STATE_ALARM) ); // is pen above Z0 or is there an alarm	
		
    // if the state has not change, we only count down to the pull time
	if (previousPenState == isPenUp) { // if state is unchanged		
		if (solenoid_pull_count > 0) {
			solenoid_pull_count--;
			solenoid_pen_pulse_len = SOLENOID_PULSE_LEN_PULL; // stay at full power while counting down
		}
		else {			
			solenoid_pen_pulse_len = SOLENOID_PULSE_LEN_HOLD; // pull in delay has expired so lower duty cycle			
		}
	}
	else { // pen direction has changed
		solenoid_pen_pulse_len = SOLENOID_PULSE_LEN_PULL; // go to full power
		solenoid_pull_count = SOLENOID_PULL_DURATION; // set the time to count down		
	}
	
	previousPenState = isPenUp; // save the prev state
	
	digitalWrite(SOLENOID_DIRECTION_PIN, isPenUp);
	
	// skip setting value if it is unchanged
	if (ledcRead(SOLENOID_CHANNEL_NUM) == solenoid_pen_pulse_len)
		return;
	
	// update the PWM value
	// ledcWrite appears to have issues with interrupts, so make this a critical section
	portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
	portENTER_CRITICAL(&myMutex);
		ledcWrite(SOLENOID_CHANNEL_NUM, solenoid_pen_pulse_len);		
	portEXIT_CRITICAL(&myMutex);
}

#endif

