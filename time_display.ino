// Include LED Dot Matrix control library
#include <LedControl.h>
// Include Wire Library Code (needed for I2C protocol devices)
#include <Wire.h>

#define DS1307_ADDRESS 0x68  // 104
// MAX_DISPLAY_VALUE it is the minimum value that is greater than one and
// shouldn't be displayed by the dot matrix.
#define MAX_DISPLAY_VALUE 60
// DATETIME_FIELDS ito track date and time a total of 6 fields are required.
// They include; Seconds, Minutes, Hours, Day, Month and Year.
#define DATETIME_FIELDS 6
// DIGIT_WIDTH defines the number of horizontal LEDs it takes to display a single digit.
#define DIGIT_WIDTH 5

#define SECONDS_DISPLAY 0x00
#define MINUTES_DISPLAY 0x01
#define HOURS_DISPLAY 0x02

// REFRESH_CYCLE redraw the screen in 500 ms (0.5 seconds)
#define REFRESH_CYCLE 500 

// DISPLAY_INTERVAL shows the time it takes to have a time type on display.
// HOURS_VALUE => 4 Seconds, MINUTES_VALUE => 3 seconds and SECONDS_VALUE => 10 Seconds.
// In 15 seconds the 3 display time types should have been displayed and restarted the 
// process all over again.
#define SECONDS_DISPLAY_INTERVAL 5
#define MINUTES_DISPLAY_INTERVAL 3
#define HOURS_DISPLAY_INTERVAL 2

// SECS_MIN_HRS_DIRECTION is the direction if the time type pixels movement from RIGHT to LEFT.
#define SECS_MIN_HRS_DIRECTION 0x01
// HRS_MIN_SECS_DIRECTION is the direction if the time type pixels movement from LEFT to RIGHT.
#define HRS_MIN_SECS_DIRECTION 0x02

int DIN = 2;
int CS = 3;
int CLK = 4;

LedControl lc = LedControl(DIN, CLK, CS, 1);

// display contains byte array input for the Max7219 for the dot matrix display per number.
byte display[][DIGIT_WIDTH] = {
  { B0111, B0101, B0101, B0101, B0111 },  // Number 0
  { B0001, B0001, B0001, B0001, B0001 },  // Number 1
  { B0111, B0001, B0111, B0100, B0111 },  // Number 2
  { B0111, B0001, B0111, B0001, B0111 },  // Number 3
  { B0101, B0101, B0111, B0001, B0001 },  // Number 4
  { B0111, B0100, B0111, B0001, B0111 },  // Number 5
  { B0111, B0100, B0111, B0101, B0111 },  // Number 6
  { B0111, B0001, B0001, B0001, B0001 },  // Number 7
  { B0111, B0101, B0111, B0101, B0111 },  // Number 8
  { B0111, B0101, B0111, B0001, B0111 },  // Number 9
};

// DoubleDotsDisplay show the direction of screen time type update for the last two rows.
byte DoubleDotsDisplay[3] = {B00000000, B00000011, B00000011};

// SingleDotDisplay show the direction of screen time type update for the last two rows.
byte SingleDotDisplay[3] = {B00000000, B00001000, B00001000};

// SecondsDisplay indicates the number of bytes shifts to be implemented to display seconds time type.
byte SecondsDisplay = 0;

// MinutesDisplay indicates the number of bytes shifts to be implemented to display minutes time type.
byte MinutesDisplay = 3;

// HoursDisplay indicates the number of bytes shifts to be implemented to display hours time type.
byte HoursDisplay = 6;

// dateTimes stores processed fields required to display date and time accurately.
// The default time set up is 01:13:00 5/December/2023
// Zero Resets seconds and start oscillator
uint16_t dateTime[DATETIME_FIELDS] = {0, 13, 1, 5, 12, 2023};

// currentDisplayType has seconds display type is set as the default initial type.
uint8_t currentDisplayType = SECONDS_DISPLAY;
// currentPxlsMovement sets the default movement of the time type pixels as from RIGHT to LEFT.
uint8_t currentPxlsMovement = SECS_MIN_HRS_DIRECTION;

// timeCounter keeps track of the time than has elapsed until all time types have been displayed.
uint16_t timeCounter;

uint8_t totalMinsInterval = SECONDS_DISPLAY_INTERVAL + MINUTES_DISPLAY_INTERVAL;
uint8_t totalHrsInterval = SECONDS_DISPLAY_INTERVAL + MINUTES_DISPLAY_INTERVAL + HOURS_DISPLAY_INTERVAL;

// the setup routine  runs once when you press reset:
void setup() {
  pinMode(DIN, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(CS, OUTPUT);


  Wire.begin();  // Join I2C bus

  // sets the initial time to the DS1307 RTC device.
  // setDatetime(dateTime);

  // the zero refers to the MAX7219 number, it is zero for 1 chip
  lc.shutdown(0, false);  // turn off power saving, enables display
  lc.setIntensity(0, 2);  // sets brightness (0~15 possible values)
  lc.clearDisplay(0);     // clear screen
}

// the loop routine runs over  and over again forever:
void loop() {
  readDatetime(); // Fetches the current time instance

  // fetchDisplayType updates the time type to display if its interval has expired.
  fetchDisplayType();

  switch(currentDisplayType) {
    case SECONDS_DISPLAY:
     outputDigit(0, dateTime[0], SecondsDisplay); // Displaying Seconds
    break;

    case MINUTES_DISPLAY:
     outputDigit(0, dateTime[1], MinutesDisplay); // Displaying Minutes
    break;

    case HOURS_DISPLAY:
     outputDigit(0, dateTime[2], HoursDisplay); // Displaying Hours
    break;
  }
 
  delay(REFRESH_CYCLE);
}

// outputDigit the double digits value provided to the max7219 matrix display.
void outputDigit(uint8_t address, uint8_t digitsToDisplay, byte shiftingPos) {
  if (digitsToDisplay >= MAX_DISPLAY_VALUE) {
    // Ignore displaying the invalid valid greater than 59.
    return;
  }

  // Only a maximum of two digits that can be displayed on the hardware thus no need to use an array.
  uint8_t numberAtPos1 = digitsToDisplay % 10;
  uint8_t numberAtPos2 = (digitsToDisplay / 10) % 10;

  // Draw the digits pixels 
  for (int i = 0; i < DIGIT_WIDTH; i++) {
    byte pixelAtPos1 = display[numberAtPos1][i];
    byte pixelAtPos2 = display[numberAtPos2][i];

    lc.setRow(address, i, (pixelAtPos2 << 4 | pixelAtPos1));
  }

  // By default the single dot binary shouldn't be shifted.
  byte newPos = B00000000;
  
  // Draw the time type pixels below.
  for (int k=0; k < sizeof(DoubleDotsDisplay); k++){
      // If shifting dot dots is done from the center (after displaying minutes) then the single dot should be changed
      if (shiftingPos == MinutesDisplay) {
        switch(currentPxlsMovement) {
          case SECS_MIN_HRS_DIRECTION:
          newPos = SingleDotDisplay[k]>>shiftingPos;
          break;

          case HRS_MIN_SECS_DIRECTION:
          newPos = SingleDotDisplay[k]<<shiftingPos;
          break;
        }
      }

      newPos = (DoubleDotsDisplay[k]<<shiftingPos) | newPos; // Join Double and Single Dot display BCD.
      lc.setRow(address, k+DIGIT_WIDTH, newPos);
  }
}

// fetchDisplayType returns the time type to be displayed on the DoT Matrix Display.
// Display only supports displaying Hours, Minutes and Seconds time types.
// Sets the correct direction of pixels movement and increment or decrease the refresh cycles.
void fetchDisplayType() {
  if (currentPxlsMovement == SECS_MIN_HRS_DIRECTION) {
    timeCounter += REFRESH_CYCLE;
  } else {
    if (timeCounter == REFRESH_CYCLE) {
      // Reset to the default pixel movement.
      currentPxlsMovement = SECS_MIN_HRS_DIRECTION;
    }
    timeCounter -= REFRESH_CYCLE;
  }
 
  // This division can't happen with the timeCounter directly as it would give zeros instead of floats.
  uint8_t timerInSeconds = timeCounter/1000;

  if (timerInSeconds < SECONDS_DISPLAY_INTERVAL ) {
      currentDisplayType = SECONDS_DISPLAY;
      return;
  } else if (timerInSeconds < totalMinsInterval) {
      currentDisplayType = MINUTES_DISPLAY;
      return;
  } else if (timerInSeconds < totalHrsInterval) {
    currentDisplayType = HOURS_DISPLAY;
    return;
  }

  // Reset the direction of pixels movement.
  currentPxlsMovement = HRS_MIN_SECS_DIRECTION;
}

// readDatetime is a routinely invoked method that returns the current time of as
// available in the DS1307 RTC chipset.
void readDatetime() {
  Wire.beginTransmission(DS1307_ADDRESS);  // Start I2C protocol with DS1307 address
  Wire.write(0);                           // Send register address
  Wire.endTransmission(false);             // I2C restart

  // request DATETIME_FIELDS bytes from peripheral device #DS1307_ADDRESS
  Wire.requestFrom(DS1307_ADDRESS, DATETIME_FIELDS);

  uint8_t i = 0;
  while (Wire.available()) {  // peripheral may send less than requested
    // This reads the shift register value stored at address defined by i.
    // i.e. Register 0 stores Seconds, register 1 stores minutes, register 2 stores Hours
    // Register 3 stores Day, Register 4 stores Month and Register 5 Stores the year.
    byte data = Wire.read();  // receive a byte.

    // Convert BCD to decimal
    dateTime[i] = (data / 16) * 10 + (data % 16);
    i++;
  }
}

// setDatetime should be called once when setting up the program. Its writes the time set to the 
// DS1307 RTC hardware.
void setDatetime(uint16_t timestamp[DATETIME_FIELDS]) {
  // Write data to DS1307 RTC
  Wire.beginTransmission(DS1307_ADDRESS);  // Start I2C protocol with DS1307 address.
  Wire.write(0); // Send register address

  // Write data for the 6 data fields
  for (int i = 0; i < DATETIME_FIELDS; i++) {
    byte data = (timestamp[i]/10) * 16 + (timestamp[i]%10);
    Wire.write(data);
  }

  Wire.endTransmission();  // Stop transmission and release the I2C bus
}
