#include <arduino.h>
#include <LedControl.h> // Include LED Dot Matrix control library
#include <Wire.h> // Include Wire Library Code (needed for I2C protocol devices)

namespace Settings
{
    // DATETIME_FIELDS ito track date and time a total of 6 fields are required.
    // They include; Seconds, Minutes, Hours, Day, Month and Year.
    constexpr int DATETIME_FIELDS {6};
    // DIGIT_WIDTH defines the number of horizontal LEDs it takes to display a single digit.
    constexpr int DIGIT_WIDTH {5};
    // REFRESH_CYCLE redraw the screen in 500 ms (0.5 seconds)
    constexpr int REFRESH_CYCLE {500}; 

    // dateTimeArray stores processed fields required to display date and time accurately.
    // The default time set up is 15:10:00  17/Nov/2024.
    int dateTimeArray[DATETIME_FIELDS] = {00, 10, 15, 17, 11, 2024};

    // display contains byte array input for the Max7219 for the dot matrix display per number.
    constexpr byte display[][Settings::DIGIT_WIDTH] = {
        { 0x07, 0x05, 0x05, 0x05, 0x07 },  // Number 0
        { 0x01, 0x01, 0x01, 0x01, 0x01 },  // Number 1
        { 0x07, 0x01, 0x07, 0x04, 0x07 },  // Number 2
        { 0x07, 0x01, 0x07, 0x01, 0x07 },  // Number 3
        { 0x05, 0x05, 0x07, 0x01, 0x01 },  // Number 4
        { 0x07, 0x04, 0x07, 0x01, 0x07 },  // Number 5
        { 0x07, 0x04, 0x07, 0x05, 0x07 },  // Number 6
        { 0x07, 0x01, 0x01, 0x01, 0x01 },  // Number 7
        { 0x07, 0x05, 0x07, 0x05, 0x07 },  // Number 8
        { 0x07, 0x05, 0x07, 0x01, 0x07 },  // Number 9
    };
};

// MatrixDisplay manages the matxrix display
class MatrixDisplay
{
  public:
    MatrixDisplay(int DIN, int CLK, int CS, int devices)
      : m_lc {DIN, CLK, CS, devices}
    {}

    void setUpDefaults()
    {
         for (int addr {0}; addr < m_lc.getDeviceCount(); ++addr)
        {
            m_lc.shutdown(addr, false);  // turn off power saving, enables display
            m_lc.setIntensity(addr, 0);  // sets brightness (0~15 possible values)
            m_lc.clearDisplay(addr);     // clear screen
        }
    }

    // displayTime outputs respective time component on to each display available.
    void displayTime()
    {
        for (int i {0}; i < m_lc.getDeviceCount(); ++i)
        {
            writeTimeToDisplay(i, Settings::dateTimeArray[i]);

            // Blink on and off
            if (Settings::dateTimeArray[0] % 2 == 0)
                writeProgressToDisplay(i, 0x18); // On State
            else 
                writeProgressToDisplay(i, 0x0); // Off State
        }
            
    }

    // writeTimeToDisplay writes the actual digits into the UI 
    void writeTimeToDisplay(int displayAddr, int digitsToDisplay)
    {
        digitsToDisplay %= digitsMask; // Filter out greater than values;

        uint8_t numberAtPos1 = digitsToDisplay % 10;
        uint8_t numberAtPos2 = (digitsToDisplay / 10) % 10;

        // Draw the digits pixels 
        for (int i {0}; i < Settings::DIGIT_WIDTH; i++) {
            byte pixelAtPos1 = Settings::display[numberAtPos1][i];
            byte pixelAtPos2 = Settings::display[numberAtPos2][i];

            m_lc.setRow(displayAddr, i, (pixelAtPos2 << 4 | pixelAtPos1));
        }
    }

    // writeProgressToDisplay draws some lively animation to show progress in Time
    // in the last two rows.
    void writeProgressToDisplay(int displayAddr, int value)
    {
        m_lc.setRow(displayAddr, 6, value); // Row 6
        m_lc.setRow(displayAddr, 7, value);  // Row 7
    }

  private:
    LedControl m_lc;

    // digitsMask defines a mask that only allows digits less than it to be
    // displayed.
    const int digitsMask {100};
};

class DateTime
{
    public:
        DateTime(bool setDefaultTime) 
        {
            Wire.begin();  // Join I2C bus

            // Sets the default time into the Timer module if true
            if (setDefaultTime)
                setDatetime();
        }

        // readDatetime is a routinely invoked method that returns the current time of as
        // available in the DS1307 RTC chipset.
        void readDatetime() {
            Wire.beginTransmission(DS1307_ADDRESS);  // Start I2C protocol with DS1307 address
            Wire.write(0);                           // Send register address
            Wire.endTransmission(false);             // I2C restart

            // request DATETIME_FIELDS bytes from peripheral device #DS1307_ADDRESS
            Wire.requestFrom(DS1307_ADDRESS, Settings::DATETIME_FIELDS);

            uint8_t i = 0;
            while (Wire.available()) {  // peripheral may send less than requested
                // This reads the shift register value stored at address defined by i.
                // i.e. Register 0 stores Seconds, register 1 stores minutes, register 2 stores Hours
                // Register 3 stores Day, Register 4 stores Month and Register 5 Stores the year.
                byte data = Wire.read();  // receive a byte.

                // Convert Hex to decimal
                Settings::dateTimeArray[i] = (data / 16) * 10 + (data % 16);
                i++;
            }
        }

    private:
        const int DS1307_ADDRESS {0x68}; // 104

        // setDatetime should be called once when setting up the program. Its writes the time set to the 
        // DS1307 RTC hardware.
        void setDatetime() {
            // Write data to DS1307 RTC
            Wire.beginTransmission(DS1307_ADDRESS);  // Start I2C protocol with DS1307 address.
            Wire.write(0); // Send register address

            // Write data for the 6 data fields
            for (int i {0}; i < Settings::DATETIME_FIELDS; i++) {
                byte data {(Settings::dateTimeArray[i]/10) * 16 + (Settings::dateTimeArray[i]%10)};
                Wire.write(data);
            }

            Wire.endTransmission();  // Stop transmission and release the I2C bus
        }
};

int main(int, char*)
{
    init();

#if defined(USBCON)
	USBDevice.attach();
#endif

    const int devices {3};
    const int DIN {10};
    const int CS {9};
    const int CLK {8};

    pinMode(DIN, OUTPUT);
    pinMode(CLK, OUTPUT);
    pinMode(CS, OUTPUT);

    MatrixDisplay md {DIN, CLK, CS, devices};
    DateTime dt {false}; // True indicates that we want to update the timer datetime.
    
    md.setUpDefaults(); // Set display defaults.
    
	while(true) {
        dt.readDatetime(); // Reads the current time into dateTimeArray.
        md.displayTime(); // Displays the time set in dateTimeArray.
        delay(Settings::REFRESH_CYCLE); 
	}
        
	return 0;
}
