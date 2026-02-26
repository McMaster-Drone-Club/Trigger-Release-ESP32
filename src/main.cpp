#include <Arduino.h>
#include <string.h>
#include <ESP32Servo.h>

#define BAUD_RATE 9600
#define COMMAND_LENGTH 4
#define DELAY_MS 500
#define TIME_MS_FOR_RELEASE 2000
#define TIME_MS_FOR_RETRACT 2000
#define SERVO_PIN 13 // change later
#define SERVO_RETRACT_ANGLE 0
#define SERVO_RELEASE_ANGLE 180
#define ANGLE_BUFFER_DEG 1

enum State {
	LOCKED,
	ARMED,
	RELEASING,
	RETRACTING,
	FAULT,
};  

enum Command {
	LOCK,
	ARM,
	RELEASE,
	STOP,
	INVALID
}; 

enum State state;

Servo servo; 

char data [COMMAND_LENGTH + 1]; 
unsigned long previousMillis; 
unsigned long currentMillis;

unsigned long timeEnteredReleasing;
unsigned long timeEnteredRetracting;

int idx = 0;


void setup() {
	Serial.begin(BAUD_RATE); 
	state = LOCKED;
	previousMillis = millis();
	servo.attach(SERVO_PIN); 
	retract(); 
}

void handle_command(Command cmd) {
	switch (cmd) {
		case LOCK:
			state = LOCKED;
			retract();
			break;

		case ARM:
			if (state == LOCKED) {
				state = ARMED; 
			}
			break;

		case RELEASE:
			if (state == ARMED) {
				state = RELEASING;
				timeEnteredReleasing = millis();
				release(); 
			}
			break;

		case STOP:
			state = FAULT; 
			retract();
			break;

		default:
			break;
	}
}

void retract() {
	if (abs(SERVO_RETRACT_ANGLE - servo.read()) > ANGLE_BUFFER_DEG) {
		servo.write(SERVO_RETRACT_ANGLE);
	}
}

void release() {
	if (abs(servo.read() - SERVO_RELEASE_ANGLE) > ANGLE_BUFFER_DEG) {
		servo.write(SERVO_RELEASE_ANGLE);
	}
}

void update_state_machine() {
	switch (state) {
		case RELEASING: 
			if (millis() - timeEnteredReleasing >= TIME_MS_FOR_RELEASE) {
				state = RETRACTING; 
				timeEnteredRetracting = millis(); 
				retract();
			}
			break;

		case RETRACTING:
			if (millis() - timeEnteredRetracting >= TIME_MS_FOR_RETRACT) {
				state = LOCKED; 
			}
			break;
	}
}

enum Command decode() {
	if (!strcmp(data, "LOCK")) {
		return LOCK;
	} else if (!strcmp(data, "ARMD")) {
		return ARM;
	} else if (!strcmp(data, "RELS")) {
		return RELEASE; 
	} else if (!strcmp(data, "STOP")) {
		return STOP; 
	} else {
		return INVALID;
	}
}


void clear_buffer(char* buffer, int size) {
	for (int i = 0; i < size; i++) {
		buffer[i] = '\0'; 
	}
}

void loop() {
	while (idx < COMMAND_LENGTH && Serial.available()) {
		int r = Serial.read(); 
		if (r < 0) { break; }
		char bit = (char)r;
		if (bit != '\n' && bit != '\r') {
			data[idx++] = bit; 
		} 
	}


	if (idx == COMMAND_LENGTH) {
		idx = 0; 
		// terminate string
		data[COMMAND_LENGTH] = '\0'; 

		// flush buffer at \n
		while (Serial.available()) {
			int r = Serial.read(); 
			char bit = (char)r;

			if (bit == '\n') {
				break;
			}
		}
		
		// decode command
		enum Command cmd = decode(); 

		// change state
		handle_command(cmd);

		// clear buffer
		clear_buffer(data, COMMAND_LENGTH + 1); 
	}

	currentMillis = millis(); 

	if (currentMillis - previousMillis >= DELAY_MS) {
		Serial.println(state); 
		previousMillis = currentMillis; 
	}
	
	update_state_machine(); 
}

