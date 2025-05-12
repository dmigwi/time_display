#include <LedControl.h>  // Include LED Dot Matrix control library
#include <Wire.h>  // Include Wire Library Code (needed for I2C protocol devices)
#include <arduino.h>

// namespace Settings
namespace Settings
{
    // DisplayMode defines the various display modes supported.
    enum DisplayMode: int {NORMAL, SET_SECONDS, SET_MINUTES, SET_HOURS, UNKNOWN};

    // isModeFlag is set to true by the interrupt triggered by the mode button.
    bool isModeFlag {false};

    // isSetFlag is set to true by the interrupt triggered by the set button.
    bool isSetFlag {false};

    // namespace Settings
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
        {0x0, 0x4, 0xC, 0x4, 0x4, 0xE, 0x0, 0x0},  // Number 1
        {0x0, 0xE, 0x2, 0xE, 0x8, 0xE, 0x0, 0x0},  // Number 2
        {0x0, 0xE, 0x2, 0xE, 0x2, 0xE, 0x0, 0x0},  // Number 3
        {0x0, 0xA, 0xA, 0xE, 0x2, 0x2, 0x0, 0x0},  // Number 4
        {0x0, 0xE, 0x8, 0xE, 0x2, 0xE, 0x0, 0x0},  // Number 5
        {0x0, 0xE, 0x8, 0xE, 0xA, 0xE, 0x0, 0x0},  // Number 6
        {0x0, 0xE, 0x2, 0x2, 0x4, 0x4, 0x0, 0x0},  // Number 7
        {0x0, 0xE, 0xA, 0xE, 0xA, 0xE, 0x0, 0x0},  // Number 8
        {0x0, 0xE, 0xA, 0xE, 0x2, 0xE, 0x0, 0x0},  // Number 9
        {0x0, 0x0, 0x8, 0x0, 0x8, 0x0, 0x0, 0x0},  // Full-colon :
        {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},  // blank
    };
};

// DateTime reads and writes to the time module.
class DateTime {
    public:
        DateTime() {
            Wire.begin();  // Join I2C bus
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

    private:
        const int DS1307_ADDRESS{0x68};  // 104

};

// MatrixDisplay manages the matxrix display
class MatrixDisplay : public DateTime {
    public:
        MatrixDisplay(int DIN, int CLK, int CS, int devices)
            :  m_lc{DIN, CLK, CS, (devices > maxDevices ? maxDevices : devices)},
               m_currentMode {Settings::DisplayMode::NORMAL}, DateTime{}
        {}

        void setUpDefaults() {
            for (int addr{0}; addr < m_lc.getDeviceCount(); ++addr) {
                m_lc.shutdown(addr, false);  // turn off power saving, enables display
                m_lc.setIntensity(addr, 0);  // sets brightness (0~15 possible values)
                m_lc.clearDisplay(addr);     // clear screen
            }
        }

        // isThinChar returns true if the value provided is thin character
        // otherwise it returns false.
        bool isThinChar(int value){
            return (value == 10 || value == 11);
        }

        // isBlink controlls the blinking function of the full colon.
        // Blinking should only happen if the seconds are even and mode is normal.
        bool isBlink(){
            return (Settings::dateTimeArray[0] & 1) && (m_currentMode == Settings::DisplayMode::NORMAL);
        }

        // displayTime outputs respective time component on to each display available.
        void displayTime() {
            int index{0};

            // Extracts the characters to displayed from the dateTimeArray and
            // formats them based on the matrix character display order.
            for (int i{0}; i < fieldsToDisplay; ++i)
            {
                int digitsToDisplay{Settings::dateTimeArray[i]};
                digitsToDisplay %= digitsMask;  // Filter out values greater than mask;

                mappedChars[index++] = digitsToDisplay % 10;  // ones position
                mappedChars[index++] = (digitsToDisplay / 10) % 10;  // tens position

                if (index != charCount)  // if not last field to display
                {
                    // Blink On and Off the Colon per second. If even, append
                    // Blank Space else append a full colon.
                    mappedChars[index++] = (isBlink() ? 10 : 11);
                }
            }

            int charWidth {0};
            int normCharWidth{4}; // Normal characters width = 4
            int thinCharWidth{2}; // Thin characters width = 2.
            unsigned long rowData{0};

            // Draw the mapped characters pixels, one row at a time.
            for (int row{0}; row < Settings::DISPLAY_ROWS; ++row)
            {
                for (auto k : mappedChars) {
                    unsigned long charRowPixels{Settings::display[k][row]};

                    // Assign thin char padding if, this char is found or if the
                    // charWith is empty.
                    charWidth += ((isThinChar(k) || charWidth == 0) ? thinCharWidth : normCharWidth);

                    rowData |= charRowPixels << charWidth;
                }

                displayRow(row, rowData);
                rowData = 0;  // clear the previous data.
                charWidth = 0; // reset the characters width.
            }
        }

        // displayRow plots display contents one row at a time for all the displays
        // supported.
        void displayRow(int rowNo, unsigned long rowData) {
            for (int i{0}; i < m_lc.getDeviceCount(); ++i)
            m_lc.setRow(i, rowNo, rowData >> (8 * i));  // extract 8 bits.
        }

        // CurrentMode returns a const reference copy of the current mode.
        const Settings::DisplayMode& CurrentMode() const {
            return m_currentMode;
        }

        // handleMode shifts the current mode to the next inline if an interrupt
        // is detected.
        void handleMode() {
            if (Settings::isModeFlag)
            {
                // Shift the current mode to the next.
                m_currentMode = static_cast<Settings::DisplayMode>(
                    (m_currentMode+1) % Settings::DisplayMode::UNKNOWN
                );
                // Set the interrupt flag to off.
                Settings::isModeFlag = false;
            }
        }

        void handleTimeSetting() {
            // Ignore further action if SetFlag is false or current mode is either
            // NORMAL or UNKNOWN.
            if (!Settings::isSetFlag || m_currentMode == Settings::DisplayMode::NORMAL ||
                m_currentMode == Settings::DisplayMode::UNKNOWN)
                return;

            int limit {60}; // Set limit to a default of 60 seconds/minutes
            if (m_currentMode == Settings::DisplayMode::SET_HOURS)
                limit = 24;

            // Increment by one and return to zero if limit is exceeded.
            Settings::dateTimeArray[m_currentMode] = (Settings::dateTimeArray[m_currentMode] + 1) % limit;
        }

    private:
        LedControl m_lc;

        // m_currentMode tracks the current mode of display.
        Settings::DisplayMode m_currentMode;

        // digitsMask defines a mask that only allows digits less than it to be
        // displayed.
        const int digitsMask{100};
        // maxDevices defines the number of displays supported simulataneously.
        const int maxDevices{4};
        // charCount is set to of 8 because:
        // Hours  => Mapped as 2 Characters
        // Colon => Mapped as 1 Characters
        // Minutes => Mapped as 2 Characters
        // Colon => Mapped as 1 Characters
        // Seconds => Mapped as 2 Characters
        static const int charCount{8};
        // mappedChars holds the DateTime characters for the matrix display.
        int mappedChars[charCount]{};
        // Only Hours, Minutes and Second should displayed as time.
        const int fieldsToDisplay{3};
};

// handleModeInterrupts set the interrupt Mode flag to true.
void handleModeInterrupts() {
    Settings::isModeFlag = true;
}

// handleSetInterrupts sets the interrupt Set flag to true.
void handleSetInterrupts() {
    Settings::isSetFlag = true;
}


int main(int, char*) {
    init();

#if defined(USBCON)
    USBDevice.attach();
#endif

    const int devices{4};
    const int DIN{10};
    const int CS{9};
    const int CLK{8};

    // Time setting pins
    const int MODE_PIN {2};
    const int SET_PIN {3};

    pinMode(DIN, OUTPUT);
    pinMode(CLK, OUTPUT);
    pinMode(CS, OUTPUT);

    pinMode(MODE_PIN, INPUT_PULLUP);
    pinMode(SET_PIN, INPUT_PULLUP);

    // Handle Mode and Set interrupts on falling edge
    attachInterrupt(MODE_PIN, handleModeInterrupts, FALLING);
    attachInterrupt(SET_PIN, handleSetInterrupts, FALLING);

    MatrixDisplay md{DIN, CLK, CS, devices};

    md.setUpDefaults();  // Set display defaults.

    while (true) {
        if(md.CurrentMode() == Settings::DisplayMode::NORMAL)
            md.readDatetime(); // Reads the current time into dateTimeArray only in NORMAL mode

        md.displayTime();   // Displays the time set in dateTimeArray.
        md.handleMode(); // increment mode.
        md.handleTimeSetting(); // increment time field

        delay(Settings::REFRESH_CYCLE);
    }

    return 0;
}
