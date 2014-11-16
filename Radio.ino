#include <Sheduler.h>
#include <IRremote.h>
#include <SPI.h>

// Led's
#define LED_STEP_SIZE 2
#define PIN_LEDS 3

// IR
#define PIN_IR A3


// Audio digital pot
#define PIN_AUDIO_SL 10
#define PIN_AUDIO_VOL A2
enum volume_source_e { EXTERNAL_SOURCE = 0, KNOB = 1 } volume_source = EXTERNAL_SOURCE;


// Sleeve digital pot
#define PIN_SLEEVE_SL A0
#define PIN_SLEEVE    A1 // This pin is set as input to shorten the mini-jack sleeve pin (used for play/pause toggling)
#define SLEEVE_ANALOG_TRESHOLD 300

typedef enum { CMD_OPEN, CMD_PLAY_PAUSE, CMD_PREV, CMD_NEXT } sleev_command_t;
#define CMD_PREV_RESISTANCE 240 // 240 Ohms
#define CMD_NEXT_RESISTANCE 600 // 600 Ohms

// Motor
#define PIN_MOTOR1 2
#define PIN_MOTOR2 4

typedef enum { MOTOR_CLOCKWISE = 1, MOTOR_ANTI_CLOCKWISE = 2, MOTOR_OFF = 0 } motor_setting_e;

typedef struct {
	uint8_t pin;
	uint8_t value;
	uint8_t target;
} LED_t;
LED_t backlight = {PIN_LEDS, 0, 0};

struct volume_t{
	uint8_t target;
	uint8_t value;
} volume = {0,0};

Sheduler sheduler;

uint8_t i;// Shared iterator
uint8_t adjust_speed = 2;

enum run_mode_e { STANDBY, RUNNING }
run_mode = STANDBY;

void mode_set (run_mode_e _mode) {
	if (_mode == STANDBY) {
		// Dim the volume
		volume_set_target(0);

		// Dim the lights
		backlight_set_target(0);

	} else if (_mode == RUNNING) {
		run_mode = RUNNING;
		backlight_randomize();
	}

	run_mode = _mode;
}

void task_backlight_rotate(Task* self){
	if (backlight.target == 0 && backlight.value == 0) {
		return;
	}

	if (backlight.target == backlight.value){
		backlight_randomize();
	}

	// If target is higher: step up
	if (backlight.target > backlight.value) {
		backlight.value += min(LED_STEP_SIZE, backlight.target - backlight.value);
	
	// If Lower: step down
	} else {
		backlight.value -= min(LED_STEP_SIZE, backlight.value - backlight.target);
	}

	backlight_write_value();
}
void backlight_set_value( uint8_t __val){
	backlight.value  = __val;
	// Serial.print("set led "); Serial.print(__led); Serial.print(" @ ");Serial.println(__val);
}
void backlight_set_target(uint8_t __val){
	backlight.target  = __val;
	volume.target = __val;
	// Serial.print("LED target: "); Serial.println(__val);
}
void backlight_write_value(){
	// Serial.print("LED write: "); Serial.println(backlight.value);
	analogWrite(backlight.pin, backlight.value);
}
void backlight_randomize(){
	backlight_set_target(random(1,255));
}

IRrecv irrecv(PIN_IR);
decode_results results;
void task_ir_read(Task* self){

	static uint32_t previous_value;

	if (irrecv.decode(&results) && results.value > 0x1FE0000 && results.value < 0x1FEFFFF ) {
		Serial.println(results.value, HEX);

		if ( results.value == ~0x0)
			results.value = previous_value;

		switch(results.value){
			case 0x1FE58A7: // MODE
				task_backlight_rotate(NULL);
				break;
			case 0x1FE609F:  // VOL_UP
				volume_increase();
				break;
			case 0x1FEA05F: // VOL_DOWN
				volume_decrease();
				break;
			case 0x1FE807F: // PLAY/PAUSE
				sleeve_command_send(CMD_PLAY_PAUSE);
				break;
			case 0x1FEC03F: // NEXT
				sleeve_command_send(CMD_NEXT);
				break;
			case 0x1FE40BF: // PREVIOUS
				sleeve_command_send(CMD_PREV);
				break;
			case 0x1FE48B7: // ON / OFF
				if( run_mode == STANDBY ){
					mode_set( RUNNING );
				}else{
					mode_set( STANDBY );
				}
				self->idle_cycles = 1500;
				break;
		}

		// Flash LED's
		analogWrite(backlight.pin, backlight.value < 64 ? 200 : 0 );
		delay(50);
		analogWrite(backlight.pin, backlight.value);

		previous_value = results.value;
		irrecv.resume(); // Receive the next value
	}
}

void volume_set_target(uint8_t __vol){
	volume.target = __vol;
}
void volume_increase(){
	if( volume.target <= (255-8)) volume.target += 8;
	volume_source = EXTERNAL_SOURCE;
}
void volume_decrease(){
	if( volume.target >= 8) volume.target -= 8;
	volume_source = EXTERNAL_SOURCE;
}

void task_volume_adjust(Task* self){
	if (volume.target == volume.value) return;

	if( volume.target > volume.value ){
		volume.value += min(adjust_speed, volume.target - volume.value);
	}else{
		volume.value -= min(adjust_speed, volume.value - volume.target);
	}

	audio_digipot_write(volume.value);
}

uint8_t stabalizer = 0; // Used for pot settling after

uint8_t read_volume_pot(){
	static int16_t last_value, value;
	if( value = analogRead(PIN_AUDIO_VOL), value < 10 || abs( last_value - value) > 10)
		last_value = value;
	
	return last_value >> 2;
}

void task_volume_adjust_pot(Task* self){
	if ( volume_source == KNOB){
		if ( run_mode != RUNNING ) return;
		volume.target = read_volume_pot();
	}else{
		uint8_t pot_value = read_volume_pot();
		if ( (pot_value >> 3) != (volume.target >> 3)){ // 8 bits  - 3 bits = 5 bits precision
			
			// Reset stabalizer
			stabalizer = 0;

			if (pot_value < volume.target){
				motor_set(MOTOR_CLOCKWISE);
			}else if (pot_value > volume.target){
				motor_set(MOTOR_ANTI_CLOCKWISE);
			}
		}else{
			motor_set(MOTOR_OFF);
			stabalizer += 1;
			if (stabalizer == 10) {
				// volume_source = KNOB;
				stabalizer = 0;
			}
		}
	}
}

bool    last_phone_connected    = false;
uint8_t phone_connected_counter = 0;

void task_detect_sleeve(Task* self){

	if ( phone_connected_counter < 5 && analogRead(PIN_SLEEVE) > SLEEVE_ANALOG_TRESHOLD ){
		phone_connected_counter += 1;
	}else if (phone_connected_counter > 0 && analogRead(PIN_SLEEVE) <= SLEEVE_ANALOG_TRESHOLD ) {
		phone_connected_counter -= 1;
	}
	// Serial.print("Phone sleeve value: "); Serial.println(analogRead(SLEEVE_SHORTING_PIN) );
	// Serial.print("Phone connected counter: "); Serial.println(phone_connected_counter);


	bool phone_connected = last_phone_connected ? (phone_connected_counter != 0) : (phone_connected_counter == 5);
	

	if ( phone_connected != last_phone_connected ){
		Serial.println(phone_connected ? "Phone connected" : "Phone not connected");
	
		if (run_mode == STANDBY && phone_connected){
			mode_set(RUNNING);
		}else if (run_mode == RUNNING && phone_connected == false){
			mode_set(STANDBY);
		}

		last_phone_connected = phone_connected;
	}
}

void print_status(Task* self){
	Serial.print("Run mode:"); Serial.println(run_mode);

	Serial.print("Volume source:"); Serial.println(volume_source);
	Serial.print("Volume target:"); Serial.println(volume.target);
	Serial.print("Volume volume:"); Serial.println(volume.value);
	Serial.print("Volume knob  :"); Serial.println((analogRead(PIN_AUDIO_VOL)));

	Serial.print("Sleeve       :"); Serial.println(analogRead(PIN_SLEEVE));
}

void setup() {
	Serial.begin(57600);

	// BACKLIGHT
	pinMode(backlight.pin, OUTPUT);
	backlight.value  = 0;
	backlight.target = 0;
	for(i = 0; i < 255; ++i){
		analogWrite(backlight.pin, i);
		delay(1);
	}
	for(i = 255; i > 0; --i){
		analogWrite(backlight.pin, i);
		delay(1);
	}
	delay(1000);



	// VOLUME DIGIPOTS
	digitalWrite(PIN_AUDIO_SL, HIGH);  // HIGH -= NOT active
	pinMode (PIN_AUDIO_SL, OUTPUT);

	// SLEEVE DIGIPOT
	digitalWrite(PIN_SLEEVE_SL, HIGH); // HIGH -= NOT active
	pinMode (PIN_SLEEVE_SL, OUTPUT);

	// SLEEVE ANALOG
	pinMode (PIN_SLEEVE, INPUT);
	digitalWrite(PIN_SLEEVE, LOW);


	// Start SPI and initialize
	SPI.begin();
	// audio_digipot_write(0);
	// sleeve_command_write(CMD_OPEN);
	// delay(3000);
	// sleeve_command_write(CMD_PLAY_PAUSE);
	// delay(3000);
	// sleeve_command_write(CMD_PREV);
	// delay(3000);
	// sleeve_command_write(CMD_NEXT);
	// delay(3000);
	// sleeve_command_write(CMD_OPEN);

	// IR
	irrecv.enableIRIn(); // Start the receiver

	// MOTOR
	Serial.print("Motor init...");
	pinMode(PIN_MOTOR1, OUTPUT);
	pinMode(PIN_MOTOR2, OUTPUT);
	Serial.print('.');
	motor_set(MOTOR_ANTI_CLOCKWISE);
	delay(1000);
	Serial.print('.');
	motor_set(MOTOR_CLOCKWISE);
	delay(500);
	Serial.print('.');
	Serial.println("DONE");



	// sheduler.addTask(task_ir_read, 100, false);

	sheduler.addTask(task_backlight_rotate, 30, false);

	sheduler.addTask(task_volume_adjust, 10, true);
	sheduler.addTask(task_volume_adjust_pot, 10, true);

	sheduler.addTask(task_detect_sleeve, 100, false);

	sheduler.addTask(print_status, 2000, true);

	// // Setup Timer1 for the sheduler,
	// // Every 1 millisecond.
	// TCCR1A  = 0x00;
	// TCCR1B  = (1 << WGM12) | (1 << CS10) | (1 << CS11); // Set prescaler to 64.
	// OCR1A   = 250;                                      // 16Mhz / 16 / 1000ms = 250 ticks.
	// TIMSK1 |= (1 << OCIE1A);                            // Set the ISR COMPA vect.

	mode_set(RUNNING);

	Serial.println("Setup done");
}

void audio_digipot_write(uint8_t value){
	// // High resistance == low volume, so flip value around
	// value = ( 255-value );

	digitalWrite(PIN_AUDIO_SL, LOW);
	if( value > 0){
		SPI.transfer(0x11); // Write to potentiometer
		SPI.transfer(value);
	}else{
		SPI.transfer(0x21); // Shutdown. Shorts B (audio out) & W (GND) together and cuts A (audio in) loose.
		SPI.transfer(0xFF); // Dummy resistance
	}
	digitalWrite(PIN_AUDIO_SL, HIGH);

	// digitalWrite(AUDIO_SLAVE_SELECT_PIN, LOW);
	// SPI.transfer(0x12); // Write data to potentiometer 1
	// SPI.transfer(value);
	// digitalWrite(AUDIO_SLAVE_SELECT_PIN, HIGH);
}

void sleeve_command_send(sleev_command_t command){
	sleeve_command_write(command);
	delay(200);
	sleeve_command_write(CMD_OPEN);
}

void sleeve_command_write(sleev_command_t command){


	Serial.print("Sleeve command "); Serial.println(command);

	digitalWrite(PIN_SLEEVE_SL, LOW);

	switch(command){
		case CMD_OPEN:
			Serial.print("OPEN "); Serial.println("-------");
			SPI.transfer(0x21); // Shutdown. Shorts B (not connected) & W (GND) together and cuts A (Sleeve) loose.
			SPI.transfer(0x0); // Dummy resistance
			pinMode(PIN_SLEEVE, INPUT);
			break;
		case CMD_PLAY_PAUSE:
			Serial.print("Pause "); Serial.println("0  ohm");
			SPI.transfer(0x11); // Write data:
			SPI.transfer(255-0);    // Short it (this makes it play/pause)
			pinMode(PIN_SLEEVE, OUTPUT);
			break;
		case CMD_PREV:
			Serial.print("PREV "); Serial.println("235 ohm");
			SPI.transfer(0x11); // Write data:
			// SPI.transfer(255*(CMD_PREV_RESISTANCE/10000));
			SPI.transfer(255-(6-2));
			pinMode(PIN_SLEEVE, INPUT);
			break;
		case CMD_NEXT:
			Serial.print("CMD_NEXT "); Serial.println("588 ohm");
			SPI.transfer(0x11); // Write data:
			SPI.transfer(255-(15-2));
			pinMode(PIN_SLEEVE, INPUT);
			break;
	}
	
	digitalWrite(PIN_SLEEVE_SL, HIGH);
}

void motor_set(motor_setting_e __setting){
	digitalWrite(PIN_MOTOR1, __setting == MOTOR_CLOCKWISE);
	digitalWrite(PIN_MOTOR2, __setting == MOTOR_ANTI_CLOCKWISE);
	// Serial.print("Direction: "); Serial.println(__setting);
}


unsigned long int last_ms = 0, ms = 0;

void loop() {
	if ( ms = millis(), ms != last_ms ){
		sheduler.tick();
		sheduler.run(); 
		last_ms = ms;
	}
}

