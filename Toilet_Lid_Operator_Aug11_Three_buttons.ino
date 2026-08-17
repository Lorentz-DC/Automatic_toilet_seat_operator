#include <Servo.h>
Servo myServo;  // Create a servo object

// Pin Configuration
const int Servo_output_pin = 3;
const int IR_sensor1_pin = 4;  // Reset position (Lid down / Home)
const int IR_sensor2_pin = 5;  // Stop position (Lid fully up / Zenith)
const int Pot_pin = A0; // Potentiometer input
const int limit_switch_pin = 13; // Critical material fail-safe
const int clockwise_output_pin = 8;    // ACTIVE LOW (CW Lift Relay)
const int counterclock_output_pin = 9;  // ACTIVE LOW (CCW Lower Relay)
const int clockwise_servo_toggle_pin = 2;
const int clockwise_toggle_pin = 6;
const int counterclock_toggle_pin = 7;
const int LED1_pin = 12;  // sensor1 green light 
const int LED2_pin = 11;  // sensor2 yellow light
const int LED3_pin = 10;  // Limit Switch warning red light


// Potentiometer range:
/* Analog values range from 0 to 1023: starting point (lowest): 1023; Max rotation (highest): 0; UPLIFT ROTATING: CLOCKWISE
resting lowest: 970; home range: 750-850; zenith range: 400-350

const int POT_REVERSE_ZENITH_TARGET = 930; //arm at zenith pushing lid down
const int POT_HOME_TARGET = 655;    // resting position
const int POT_ZENITH_TARGET = 225;  // fully lift up 
const int POT_BALLPARK_TOLERANCE = 40; // 40-digit deadband tolerance for belt slop
*/

// // --- State management ---
enum SystemState { 
STATE_FLOAT, 
STATE_HOME, 
STATE_Servo_latch,
STATE_LIFTING, 
STATE_PAUSED, 
STATE_LIFTING_RECOVER, 
STATE_REVERSE, 
STATE_REVERSE_PAUSED, 
STATE_REVERSE_RECOVER,
STATE_MANUAL_RESET
};
// NEW: System boots into FLOAT state to handle initial position confirmation safely
SystemState current_state = STATE_FLOAT;  

// NEW: Ballpark tracking condition outputs
/*
boolean pot_at_home = false;
boolean pot_at_zenith = false;
boolean pot_at_reverse_zenith = false;
*/

boolean sensor1_activate = false;
boolean sensor2_activate = false;

boolean Clockwise_direction = false;
boolean Counterclock_direction = false;
boolean Motion_activate = false;

boolean Clockwise_toggle = false;
boolean Counterclock_toggle = false;

//clockwise toggle
boolean old_input_state_clockwise = HIGH;
bool current_button_state_clockwise= HIGH;  

boolean old_input_state_servo = HIGH;
boolean current_input_state_servo = HIGH;

unsigned long servo_start_time = 0;
bool servo_timer_state = false;


//counterclock toggle 
boolean old_input_state_counterclock = HIGH;

boolean Servo_latch = false;
boolean Servo_liftdown = false;
boolean blink_sequence = false;
boolean LED_blink_state = LOW; 

// --- Non-blocking timers --- 
unsigned long previousMillis = 0;
const long blink_interval = 500; // Frequency of Serial updates and reading (250ms)
const long print_interval = 50;
// Non-blocking pause tracking variables
unsigned long pause_start_time = 0;
const unsigned long pause_duration = 3000; // Safe 0.5-second electrical discharge window
// NEW: Boot/Homing timeout guard variable
const unsigned long servo_interval = 500;


bool IR_Sensors() {
  // Read raw inputs as fast as hardware allows
  bool sensor_reading_1 = digitalRead(IR_sensor1_pin);
  bool sensor_reading_2 = digitalRead(IR_sensor2_pin);
  // Evaluate Sensor 1 (Instantaneous response)
  digitalWrite(LED1_pin, !sensor_reading_1);
  sensor1_activate = !sensor_reading_1;
  // Evaluate Sensor 2 (Instantaneous response)
  digitalWrite(LED2_pin, !sensor_reading_2);
  sensor2_activate = !sensor_reading_2;
}

bool Limit_Switch() {
  // Hard Failure Limit Switch Guard
  if (digitalRead(limit_switch_pin) == LOW) {
    digitalWrite(LED3_pin, HIGH);
    Clockwise_direction = false;
    Counterclock_direction = false; //stop all movements
    Motion_activate = false;
    current_state = STATE_FLOAT; //declare warning state
  }
  if (digitalRead(limit_switch_pin) == HIGH && current_state != STATE_FLOAT) {
    digitalWrite(LED3_pin, LOW);
  }
}


bool Toggle_Op_Clockwise_main() {
  // Global/Static memory states that survive frame cycles

  // Read and debounce input pin
  bool new_input_state_clockwise1 = digitalRead(clockwise_toggle_pin); 
  delay(2);
  bool new_input_state_clockwise2 = digitalRead(clockwise_toggle_pin); 
  delay(2);
  bool new_input_state_clockwise3 = digitalRead(clockwise_toggle_pin);

  if ((new_input_state_clockwise1 == new_input_state_clockwise2) && (new_input_state_clockwise1 == new_input_state_clockwise3)) {
    current_button_state_clockwise = new_input_state_clockwise3;
  }

  // --- Step 1: Detect Falling Edge (New Button Press) ---
  if (current_button_state_clockwise!= old_input_state_clockwise){

      if (current_button_state_clockwise == LOW){
        if (current_state == STATE_FLOAT){
        Clockwise_toggle = true;
        }

        else if(current_state == STATE_HOME) {
         Clockwise_toggle = true;//consume in state_ops

            }//current_button_state_clockwise== LOW
      }//current_button_state_clockwise == LOW

   old_input_state_clockwise = current_button_state_clockwise; // Save edge state
  }//(current_button_state_clockwise!= old_input_state_clockwise)

      // Arm movement handles staging in the State_Operations pipeline next!
}//function bracket

bool Toggle_Op_Clockwise_Servo(){

bool new_input_state_servo1 = digitalRead(clockwise_servo_toggle_pin); 
  delay(2);
  bool new_input_state_servo2 = digitalRead(clockwise_servo_toggle_pin); 
  delay(2);
  bool new_input_state_servo3 = digitalRead(clockwise_servo_toggle_pin);

  if ((new_input_state_servo1 == new_input_state_servo2) && (new_input_state_servo1 == new_input_state_servo3)) {
    current_input_state_servo = new_input_state_servo3;
  }

  // --- Step 1: Detect Falling Edge (New Button Press) ---
  if (current_input_state_servo!= old_input_state_servo){

      if (current_input_state_servo == LOW){

        if(current_state == STATE_HOME) {
         Servo_latch = true;//consume in state_ops

            }//current_button_state_clockwise== LOW
      }//current_button_state_clockwise == LOW

   old_input_state_servo = current_input_state_servo; // Save edge state
  }//(current_button_state_clockwise!= old_input_state_clockwise)


}

bool Toggle_Op_Counterclock() {
  bool new_input_state_counterclock1 = digitalRead(counterclock_toggle_pin);
  delay(2);
  bool new_input_state_counterclock2 = digitalRead(counterclock_toggle_pin);
  delay(2);
  bool new_input_state_counterclock3 = digitalRead(counterclock_toggle_pin);
 
  // if all 3 values are the same we can continue
  if ((new_input_state_counterclock1 == new_input_state_counterclock2) && (new_input_state_counterclock1 == new_input_state_counterclock3)) {
    if (new_input_state_counterclock3 != old_input_state_counterclock){
       old_input_state_counterclock = new_input_state_counterclock3;

       if ((new_input_state_counterclock3 == LOW && current_state == STATE_HOME)||(new_input_state_counterclock3 == LOW && current_state == STATE_FLOAT)){
          Counterclock_toggle = true;//consume in state_ops
       }
       
       else if (new_input_state_counterclock3 == HIGH){
        Counterclock_toggle = false;
      }  
  }
}
}//FUNCTION
// NEW: Dual-redundancy analog window tracker
/* void Rotation_Tracking() {
  int Pot_reading = analogRead(Pot_pin);
  
  // Evaluate ballpark position windows using deadband tolerances
  pot_at_home = (abs(Pot_reading - POT_HOME_TARGET) <= POT_BALLPARK_TOLERANCE);
  pot_at_zenith = (abs(Pot_reading - POT_ZENITH_TARGET) <= POT_BALLPARK_TOLERANCE);
  pot_at_reverse_zenith = (abs(Pot_reading - POT_REVERSE_ZENITH_TARGET) <= POT_BALLPARK_TOLERANCE);
}
*/
bool Manual_Reset(){
  if (current_state == STATE_FLOAT){
    if(Clockwise_toggle == true||Counterclock_toggle == true){
      current_state = STATE_MANUAL_RESET;
  } //clockwise reset
  }
}//function bracket

void State_Operations() {
  // NEW Phase 0: Safe On-Boot Floating State Homing Routine
 
    /* POT_REVERSE_ZENITH_TARGET = 930; POT_HOME_TARGET = 655;  POT_ZENITH_TARGET = 225; POT_BALLPARK_TOLERANCE = 40; 
    SystemStates: STATE_FLOAT, STATE_HOME, STATE_LIFTING, STATE_PAUSED, STATE_LIFTING_RECOVER, STATE_WARNING 

    if reverse range, clockwise until home sensor on; 
    if home range: (uncertain states where home sensor is not on) if the initial position is under the home sensor, rotate clockwise until home sensor is on;
    if the initial position is above the homesensor, turn clockwise and no sensor turns on and/or zenith sensor turns on or rotation is already in life/reverse range, go back counterclockwise until home sensor is on;
    if in lift range or danger zone, turn counterclockwise until home sensor is on. 
    */
    // If the arm is already happily at home on startup, exit float mode instantly
  if (sensor1_activate == true && 
      current_state != STATE_LIFTING && 
      current_state != STATE_REVERSE &&
      current_state != STATE_Servo_latch){

      current_state = STATE_HOME;
       // Only accept user inputs if the initialization float/homing phase is done!
  }

  if (current_state == STATE_FLOAT){ //declare warning state)
    Motion_activate = false;
    Clockwise_direction = false;
    Counterclock_direction = false;
  }

  switch (current_state) {
    case STATE_MANUAL_RESET: 
        if (Clockwise_toggle == true){
        Clockwise_toggle = false; //consume token
        Clockwise_direction = true;//authorize direction;
        Motion_activate = true;
        }
        else if(Counterclock_toggle == true){
        Counterclock_toggle = false;//consume token
        Counterclock_direction = true;
        Motion_activate = true;
        }

      if(sensor1_activate == true){
      Clockwise_direction = false;
      Counterclock_direction = false;
      Motion_activate = false;
      current_state = STATE_HOME;
      }
    break;

    case STATE_HOME:                            //
      Motion_activate = false;                  // Enforce absolute idle silence
      Clockwise_direction = false;       //
      Counterclock_direction = false;     //
      Servo_liftdown = false;
        if (Servo_latch == true){
          current_state = STATE_Servo_latch;
          Servo_latch = false; //consume token
        }
        else if (Clockwise_toggle == true) {           //
        Clockwise_toggle = false;               // Consume token safely
        current_state = STATE_LIFTING;          //
        } 
        else if (Counterclock_toggle == true) {   //
        Counterclock_toggle = false;            // Consume token
        current_state = STATE_REVERSE;          //
        }
    break;

    case STATE_LIFTING:
      Clockwise_direction = true;
      Motion_activate = true;

        if (sensor2_activate == true) {
          Clockwise_direction = false;
          Counterclock_direction = false;
          Motion_activate = false;
          pause_start_time = millis(); 
          current_state = STATE_PAUSED; 
    }
    break;

    case STATE_PAUSED:
     if (millis() - pause_start_time >= pause_duration) {
      current_state = STATE_LIFTING_RECOVER;
    }
    break;

    case STATE_LIFTING_RECOVER:
     Clockwise_direction = false;
    Counterclock_direction = true;
    Motion_activate = true;

     if (sensor1_activate == true) {
      Clockwise_direction = false;
      Counterclock_direction = false;
      Motion_activate = false;
      current_state = STATE_HOME;
     }
    break;

  case STATE_REVERSE:                       //
      Clockwise_direction = false;       //
      Counterclock_direction = true;     // Force motor downwards
      Motion_activate = true;   
      Servo_liftdown = true;              //

      if (sensor2_activate == true) { //
        Motion_activate = false; //
        Counterclock_direction = false; //
        pause_start_time = millis(); //
        current_state = STATE_REVERSE_PAUSED; //
          }
    break;

    case STATE_REVERSE_PAUSED: //
        if (millis() - pause_start_time >= pause_duration) { //
            current_state = STATE_REVERSE_RECOVER; //
            }
      break;

    case STATE_REVERSE_RECOVER: //
          Clockwise_direction = true; // Pull back home
          Counterclock_direction = false; //
          Motion_activate = true; //

            if (sensor1_activate == true) { //
            Motion_activate = false; //
              Clockwise_direction = false; //
              current_state = STATE_HOME; //
              }
          break;
  

  case STATE_Servo_latch:
   Servo_liftdown = true;//consume in state_ops
   if (servo_timer_state == false){
      servo_start_time = millis(); // Benchmark time for the motor delay window
      servo_timer_state = true;}

    if(servo_timer_state == true && millis()-servo_start_time >= servo_interval){
    servo_timer_state = false;
    servo_start_time = 0;
    current_state = STATE_LIFTING; //consume in state_ops
  }
  break;

/*

  /*else if {
      // Timeout supervisor: if the motor drives backward for 5 seconds and finds nothing, shut it down
      if (millis() - float_start_time > FLOAT_TIMEOUT_MAX) {
        digitalWrite(clockwise_output_pin, HIGH);
        digitalWrite(counterclock_output_pin, HIGH);
        Serial.println("CRITICAL ERROR: Homing Timeout! Check sensors or physical arm jams.");
        while(1); // Permanent safe microcontroller loop lock
      }
    }*/
    

  }//switch bracket
}//function bracket

bool Initialization(){ //runs once on start-up

    Clockwise_direction = false;
    Counterclock_direction = false;
    Motion_activate = false;
    Servo_liftdown = false;

    if (sensor1_activate == true) {
      current_state = STATE_HOME;
      Serial.println("Initial System State AT HOME, System good to go");
      Serial.println("Pot: "); Serial.println(analogRead(Pot_pin));
      Serial.print("   State: "); Serial.println(current_state);
    }
    else{
      current_state = STATE_FLOAT;
      Serial.println("Initial System State FLOATING, MANUAL RESET REQUIRED");
      Serial.println("Pot: "); Serial.println(analogRead(Pot_pin));
      Serial.print("   State: "); Serial.println(current_state);
    }
}

bool Blinking_Sequence(){
if (current_state == STATE_FLOAT){
    blink_sequence = true;
     unsigned long currentMillis = millis();
    if (currentMillis - previousMillis > blink_interval) {
      previousMillis = currentMillis; 
      if (LED_blink_state == LOW){
        LED_blink_state = HIGH;}
      else {
        LED_blink_state = LOW;
      }
    digitalWrite(LED3_pin, LED_blink_state);
    }
}
else{
  blink_sequence = false;
}
}

void setup() {
  Serial.begin(115200); 

  myServo.attach(Servo_output_pin);  // Attaches the servo on pin 3

  // Pull-up protections on inputs to kill motor EM field brush disruptions
  pinMode(IR_sensor1_pin, INPUT);
  pinMode(IR_sensor2_pin, INPUT);
  pinMode(Pot_pin, INPUT);
  pinMode(limit_switch_pin, INPUT_PULLUP);
  pinMode(clockwise_toggle_pin, INPUT_PULLUP);
  pinMode(counterclock_toggle_pin, INPUT_PULLUP);

  pinMode(clockwise_output_pin, OUTPUT);
  pinMode(counterclock_output_pin, OUTPUT);
  pinMode(LED1_pin, OUTPUT);
  pinMode(LED2_pin, OUTPUT);
  pinMode(LED3_pin, OUTPUT);

  // Active-Low Ground state: Lock them off immediately on boot
  digitalWrite(clockwise_output_pin, HIGH); 
  digitalWrite(counterclock_output_pin, HIGH); 
  Counterclock_direction = false;
  Clockwise_direction = false;
  Motion_activate = false;
  digitalWrite(LED1_pin, LOW);
  digitalWrite(LED2_pin, LOW);
  digitalWrite(LED3_pin, LOW);

  //initialization 
  IR_Sensors();
  //Rotation_Tracking();
  Initialization();
  myServo.write(5);

  delay(100); // Give the 5V power rail 50ms to fully stabilize
  old_input_state_clockwise = digitalRead(clockwise_toggle_pin); 
  
}



void loop() {
  IR_Sensors();
  //Rotation_Tracking(); // NEW: Track the analog potentiometer position concurrently
  Limit_Switch();
  Toggle_Op_Clockwise_main();
  Toggle_Op_Clockwise_Servo();
  Toggle_Op_Counterclock();
  //STATE_DETECTION();
  Blinking_Sequence();
  Manual_Reset();
  State_Operations();

 //-------------------------------------------------------
 //SystemStates: [STATE_FLOAT, STATE_HOME, STATE_LIFTING, STATE_PAUSED, STATE_LIFTING_RECOVER, STATE_REVERSE, STATE_REVERSE_PAUSED, STATE_REVERSE_RECOVER, STATE_WARNING]
    if(Servo_liftdown == true){
      myServo.write(159);
    }
    else if(Servo_liftdown == false){
      myServo.write(5);
    }


  if (Clockwise_direction == true && Counterclock_direction == false && Motion_activate == true) { 
    digitalWrite(clockwise_output_pin, LOW);     // CW Relay ON (Drive Up)
    digitalWrite(counterclock_output_pin, HIGH);    // CCW Relay OFF
  }
 else if (Clockwise_direction == false && Counterclock_direction == true && Motion_activate == true) { 
    digitalWrite(clockwise_output_pin, HIGH);     // CW Relay ON (Drive Up)
    digitalWrite(counterclock_output_pin, LOW);    // CCW Relay OFF
  }
  else {
     digitalWrite(clockwise_output_pin, HIGH);     // CW Relay ON (Drive Up)
    digitalWrite(counterclock_output_pin, HIGH);    // CCW Relay OFF
  }

 //--------------------------------------------------------

  // Live serial telemetry outputs
  if ((Clockwise_direction == true && Counterclock_direction == false && Motion_activate == true) ||(Clockwise_direction == false && Counterclock_direction == true && Motion_activate == true)){
  Serial.println("Pot: "); Serial.println(analogRead(Pot_pin));
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= print_interval) {
    previousMillis = currentMillis;
    Serial.print("   State: "); Serial.println(current_state);
    }

  }
}
