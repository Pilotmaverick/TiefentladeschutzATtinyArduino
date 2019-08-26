#include <avr/sleep.h>
#include <avr/wdt.h>

//Set Pins
const int pinMosfet = 0;
const int pinVoltage = A1;
const int pinDebug = 4;
const int pinEnableVoltage = 1;

//Set Constants
//Voltage divider: R1 = 27kOhm; R2 = 10kOhm; 0V = 0V; 5V = 18,5V
const long shutdownVoltage = 12000L; //mV
const long hysteresis = 1000L; //mV
const long upperVoltage = 15714L; //mV
const int wdLoopCounter = 1; //Multiplier to the WD-Timer: loopCouter = 8 --> 8 x 8s = 64s to next wakeup

//Set Variables
volatile int wdCount = 0;
long voltageRaw = 0L;
long voltageMapped = 0L;
bool underVoltageTrip = false;
bool voltageWarningIssued = false;

//Prototypes
void goToSleep(void);
void blinkLed(int x, int y);
void blinkValue(long val);

//Setup
void setup() {
  //Set PinMode
  pinMode(pinMosfet, OUTPUT);
  pinMode(pinEnableVoltage, OUTPUT);
  pinMode(pinVoltage, INPUT);
  pinMode(pinDebug, OUTPUT); //Debugging only

  //Set WD-Timer & Sleep mode
  cli(); //Disable all interrupts
  wdt_reset(); // Reset Watchdog Timer
  MCUSR &= ~(1 << WDRF); //reset WD-System reset flag
  WDTCR = (1 << WDCE) | (1 << WDE); //Watchdog Change Enable
  WDTCR = (1 << WDP3) | (1 << WDP0); //WD-Timer set to 8s
  WDTCR |= (1 << WDTIE); //WD-Mode = Interrupt Mode
  sei(); //Enable all interrupts

  //Set pinEnableVoltage HIGH so it disconnects the voltage divider from the Analog Input pin to save power(P-Channel MOSFET)
  digitalWrite(pinEnableVoltage, HIGH);
  
  goToSleep();
}

//Main Loop
void loop() {
  if (wdCount >= wdLoopCounter) {
    //Connect the voltage divider to the ADC
    digitalWrite(pinEnableVoltage, LOW);

    //Read Analog Input
    delay(500); //Let the analogRead settle bevor reading it
    voltageRaw = analogRead(pinVoltage);

    //Disconnect the voltage divider from the ADC
    digitalWrite(pinEnableVoltage, HIGH);

    //Calculate Voltage based on analog read
    voltageMapped = (voltageRaw * upperVoltage) / 1023L;

    blinkLed(10, 50);
    blinkValue(voltageMapped);
    blinkLed(10, 50);

    //Voltage has dipped under the Voltagelimit
    if (voltageMapped < shutdownVoltage) {
      //If Voltage lower Trip Voltage --> open MOSFET
      underVoltageTrip = true;
      blinkLed(2, 200);
    }
    //Voltage has recovered but not over the hysteresis
    else if ((voltageMapped < (shutdownVoltage + hysteresis)) && underVoltageTrip) {
      //If MOSFET opened AND Voltage lower Hysteresis --> open MOSFET
      underVoltageTrip = true;
      blinkLed(4, 200);
    }
    //Voltage gets close to Shutdown --> Warning by flashing the light
    else if ((voltageMapped < (shutdownVoltage + hysteresis)) && !underVoltageTrip && !voltageWarningIssued){
      digitalWrite(pinMosfet, false);
      delay(500);
      digitalWrite(pinMosfet, true);
      delay(500);
      digitalWrite(pinMosfet, false);
      delay(500);
      digitalWrite(pinMosfet, true);

      voltageWarningIssued = true;
    }
    //If voltage higher than hysteresis voltage or higher then shutdown voltage and MOSFET not open then --> MOSFET cloesd
    else {      
      underVoltageTrip = false;

      //Reset flag warningissued
      if ((voltageMapped > (shutdownVoltage + hysteresis + hysteresis)) && voltageWarningIssued){
        voltageWarningIssued = false;
      }
    }

    //Set MOSFET output
    digitalWrite(pinMosfet, !underVoltageTrip);
    digitalWrite(pinDebug, !underVoltageTrip);

    //Reset loop Counter
    wdCount = 0;

    goToSleep();
  }
  else {
    goToSleep();
  }
}

//Sleep
void goToSleep() {
  byte adcsra;

 // adcsra = ADCSRA; //Saving ADC Control and Status Register A
 // ADCSRA &= ~(1 << ADEN); //Disable ADC
  MCUCR |= (1 << SM1) & ~(1 << SM0);//Sleep-Mode = Power Down
  MCUCR |= (1 << SE); //Set Sleep Enable
  sleep_cpu(); //sleep
  MCUCR &= ~(1 << SE); //Set Sleep Disable
 // ADCSRA = adcsra; //Set back ADC Control and Statur Register A
}

//Interrupt routine
ISR(WDT_vect) {
  wdCount += 1;
}

void blinkLed(int x, int y){
  for (int i = 0; i < x; i++) {
    digitalWrite(pinDebug, HIGH);
    delay(y);
    digitalWrite(pinDebug, LOW);
    delay(y);
  }
}

void blinkValue(long val){
  long valRtl = 0; 
  for (int i = 0; i < 5; i++){
    valRtl += (val % 10);
    val /= 10;
    valRtl *= 10;
  }
  for (int i = 0; i < 5; i++){
    if ((valRtl % 10) == 0) {
      blinkLed(5, 50);
    }
    blinkLed((valRtl % 10), 200);
    valRtl /= 10;
    delay(2000);
  }
}
