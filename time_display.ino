#include <LedControl.h>  // Include LED Dot Matrix control library
#include <Wire.h>  // Include Wire Library Code (needed for I2C protocol devices)
#include <arduino.h>

namespace Settings 
{
    // DATETIME_FIELDS ito track date and time a total of 6 fields are required.
    // They include; Seconds, Minutes, Hours, Day, Month and Year.
    constexpr int DATETIME_FIELDS{6};
    // DISPLAY_ROWS defines the max number of horizontal LEDs supported by the
    // display.
    constexpr int DISPLAY_ROWS{8};
    // REFRESH_CYCLE redraw the screen in 500 ms (0.5 seconds)
    constexpr int REFRESH_CYCLE{500};

    // dateTimeArray stores processed fields required to display date and time
    // accurately. The default time set up is 13:32:10  23/Nov/2024.
    int dateTimeArray[DATETIME_FIELDS] = {10, 32, 13, 23, 11, 2024};

    // display contains byte array input for the Max7219 for the dot matrix display
    // per number.
    constexpr byte display[][Settings::DISPLAY_ROWS] = {
        {0x0, 0xE, 0xA, 0xA, 0xA, 0xE, 0x0, 0x0},  // Number 0
        {0x0, 0x8, 0x8, 0x8, 0x8, 0x8, 0x0, 0x0},  // Number 1
        {0x0, 0xE, 0x2, 0xE, 0x8, 0xE, 0x0, 0x0},  // Number 2
        {0x0, 0xE, 0x2, 0xE, 0x2, 0xE, 0x0, 0x0},  // Number 3
        {0x0, 0xA, 0xA, 0xE, 0x2, 0x2, 0x0, 0x0},  // Number 4
        {0x0, 0xE, 0x8, 0xE, 0x2, 0xE, 0x0, 0x0},  // Number 5
        {0x0, 0xE, 0x8, 0xE, 0xA, 0xE, 0x0, 0x0},  // Number 6
        {0x0, 0xE, 0x2, 0x2, 0x2, 0x2, 0x0, 0x0},  // Number 7
        {0x0, 0xE, 0xA, 0xE, 0xA, 0xE, 0x0, 0x0},  // Number 8
        {0x0, 0xE, 0xA, 0xE, 0x2, 0xE, 0x0, 0x0},  // Number 9
        {0x0, 0x0, 0x8, 0x0, 0x8, 0x0, 0x0, 0x0},  // Full-colon :
        {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},  // blank
    };

    // isThinChar returns true if the value provided is thin character
    // otherwise it returns false. 
    bool isThinChar(int value)
    {
        return (value == 1 || value == 10 || value == 11);
    }
};  // namespace Settings

// MatrixDisplay manages the matxrix display
class MatrixDisplay {
    public:
        MatrixDisplay(int DIN, int CLK, int CS, int devices)
            : m_lc{DIN, CLK, CS, (devices > maxDevices ? maxDevices : devices)} {}

        void setUpDefaults() {
            for (int addr{0}; addr < m_lc.getDeviceCount(); ++addr) {
                m_lc.shutdown(addr, false);  // turn off power saving, enables display
                m_lc.setIntensity(addr, 0);  // sets brightness (0~15 possible values)
                m_lc.clearDisplay(addr);     // clear screen
            }
        }

        // displayTime outputs respective time component on to each display available.
        void displayTime() {
            // charCount is set to of 8 because:
            // Hours  => Mapped as 2 Characters
            // Colon => Mapped as 1 Characters
            // Minutes => Mapped as 2 Characters
            // Colon => Mapped as 1 Characters
            // Seconds => Mapped as 2 Characters
            //  2:1:2:1:2
            const int charCount{8};

            // Only Hours, Minutes and Second should displayed as time.
            const int fieldsToDisplay{3};

            int mappedChars[charCount]{};
            int index{0};

            for (int i{0}; i < fieldsToDisplay; ++i)
            {
                int digitsToDisplay{Settings::dateTimeArray[i]};
                digitsToDisplay %= digitsMask;  // Filter out values greater than mask;

                mappedChars[index++] = digitsToDisplay % 10;  // ones position
                mappedChars[index++] = (digitsToDisplay / 10) % 10;  // tens position

                if (index < charCount)  // if not last field to display
                {
                    // Blink On and Off the Colon per second
                    if (Settings::dateTimeArray[0] % 2 == 0)
                        mappedChars[index++] = 10;  // Append Colon
                    else
                        mappedChars[index++] = 11;  // Append Blank Space
                }
            }

            unsigned long rowData{0};
            // Most characters use a width of 4 bits on the display. Some characters
            // require less.
            int normCharWidth{4};
            // Characters like (1) and (:) only need 2 bits width on the display.
            int thinCharWidth{2};

            int countThinChars{0};
            for (auto k : mappedChars) // count thin chars.
                if (Settings::isThinChar(k)) ++countThinChars;

            int charWidth {0};
            // Draw the mapped characters pixels, one row at a time.
            for (int row{0}; row < Settings::DISPLAY_ROWS; ++row) 
            {
                for (auto k : mappedChars) {
                    unsigned long charRowPixels{Settings::display[k][row]};

                    if (charWidth == 0) // if first char, add thin chars padding removed.
                        charWidth += countThinChars;
                    else
                    {
                        if (Settings::isThinChar(k))
                            charWidth += thinCharWidth;
                        else
                            charWidth += normCharWidth;
                    }

                    rowData |= charRowPixels << charWidth;
                }

                displayRow(row, rowData);
                rowData = 0;  // clear the previous data.
                charWidth = 0; // reset the character width.
            }
        }

        // displayRow plots display contents one row at a time for all the displays
        // supported.
        void displayRow(int rowNo, unsigned long rowData) {
            for (int i{0}; i < m_lc.getDeviceCount(); ++i)
            m_lc.setRow(i, rowNo, rowData >> (8 * i));  // extract 8 bits.
        }

    private:
        LedControl m_lc;

        // digitsMask defines a mask that only allows digits less than it to be
        // displayed.
        const int digitsMask{100};
        // maxDevices defines the number of displays supported simulataneously.
        const int maxDevices{4};
};

class DateTime {
    public:
        DateTime(bool setDefaultTime) {
            Wire.begin();  // Join I2C bus

            // Sets the default time into the Timer module if true
            if (setDefaultTime) setDatetime();
        }

        // readDatetime is a routinely invoked method that returns the current time of
        // as available in the DS1307 RTC chipset.
        void readDatetime() {
            // Start I2C protocol with DS1307 address
            Wire.beginTransmission( DS1307_ADDRESS);
            Wire.write(0);                // Send register address
            Wire.endTransmission(false);  // I2C restart

            // request DATETIME_FIELDS bytes from peripheral device #DS1307_ADDRESS
            Wire.requestFrom(DS1307_ADDRESS, Settings::DATETIME_FIELDS);

            int i = 0;
            while (Wire.available()) 
            {  
                // peripheral may send less than requested
                // This reads the shift register value stored at address defined by i.
                // i.e. Register 0 stores Seconds, register 1 stores minutes, register 2
                // stores Hours Register 3 stores Day, Register 4 stores Month and
                // Register 5 Stores the year.
                byte data = Wire.read();  // receive a byte.

                // Convert Hex to decimal
                Settings::dateTimeArray[i] = (data / 16) * 10 + (data % 16);
                i++;
            }
        }

    private:
        const int DS1307_ADDRESS{0x68};  // 104

        // setDatetime should be called once when setting up the program. Its writes
        // the time set to the DS1307 RTC hardware.
        void setDatetime() {
            // Write data to DS1307 RTC. Start I2C protocol with DS1307 address.
            Wire.beginTransmission(DS1307_ADDRESS);
            Wire.write(0);        // Send register address

            // Write data for the 6 data fields
            for (int i{0}; i < Settings::DATETIME_FIELDS; i++) {
                byte data{(Settings::dateTimeArray[i] / 10) * 16 +
                            (Settings::dateTimeArray[i] % 10)};
                Wire.write(data);
            }

            Wire.endTransmission();  // Stop transmission and release the I2C bus
        }
};

int main(int, char*) {
    init();

#if defined(USBCON)
    USBDevice.attach();
#endif

    const int devices{4};
    const int DIN{10};
    const int CS{9};
    const int CLK{8};

    pinMode(DIN, OUTPUT);
    pinMode(CLK, OUTPUT);
    pinMode(CS, OUTPUT);

    // True indicates that we want to update the timer datetime.
    bool resetTime{false};

    MatrixDisplay md{DIN, CLK, CS, devices};
    DateTime dt{resetTime};

    md.setUpDefaults();  // Set display defaults.

    while (true) {
        dt.readDatetime();  // Reads the current time into dateTimeArray.
        md.displayTime();   // Displays the time set in dateTimeArray.
        delay(Settings::REFRESH_CYCLE);
    }

    return 0;
}
