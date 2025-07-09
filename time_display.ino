#include <LedControl.h>  // Include LED Dot Matrix control library
#include <Wire.h>  // Include Wire Library Code (needed for I2C protocol devices)
#include <arduino.h>

// namespace Settings
namespace Settings
{
    // DisplayMode defines the various display modes supported.
    enum DisplayMode: int {
        SET_SECONDS, SET_MINUTES, SET_HOURS, // SET_TIME modes
        NORMAL, UNKNOWN // Control modes
    };

    // isModeFlagOn is set to true by the interrupt triggered by the mode button.
    volatile bool isModeFlagOn {false};

    // isSetFlagOn is set to true by the interrupt triggered by the set button.
    volatile bool isSetFlagOn {false};

    // isI2CActive is set to true when the i2C communication with DS1307RTC chipset
    // is in progress otherwise false.
    volatile bool isI2CActive {false};

    // lastModeTime defines the last timestamped recorded when the Mode
    // interrupt was activated.
    volatile unsigned long lastModeTime = 0;

    // lastSetTime defines the last timestamped recorded when the Set
    // interrupt was activated.
    volatile unsigned long lastSetTime = 0;

    // DEBOUNCE_DELAY defines the time in milliseconds that a mechanical switch
    // has to wait be triggering an actual button click.
    constexpr int DEBOUNCE_DELAY {1000};

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
    // accurately. The default time set up is 02:32:11  20/May/2025.
    int dateTimeArray[DATETIME_FIELDS] = {11, 32, 12, 20, 05, 2025};

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
            // Activate the I2C Flag before initiating the i2c communication.
            Settings::isI2CActive = true;

            // Start I2C protocol with DS1307 address
            Wire.beginTransmission(DS1307_ADDRESS);
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
                auto elem = div(data, 16);
                Settings::dateTimeArray[i] = elem.quot * 10 + elem.rem;
                i++;
            }

            // Disable i2c flag after the I2C communication is over.
            Settings::isI2CActive = false;
        }

        // setDatetime should be called once when setting up the program. Its writes
        // the time set to the DS1307 RTC hardware.
        void setDatetime() {
            // Activate the I2C Flag before initiating the i2c communication.
            Settings::isI2CActive = true;

            // Write data to DS1307 RTC. Start I2C protocol with DS1307 address.
            Wire.beginTransmission(DS1307_ADDRESS);
            Wire.write(0x00);        // Send register address

            // Write data for the 6 data fields
            for (int i{0}; i < Settings::DATETIME_FIELDS; i++) {
                auto data = div(Settings::dateTimeArray[i], 10);
                Wire.write(data.quot * 16 + data.rem);
            }

            Wire.endTransmission();  // Stop transmission and release the I2C bus

            // Disable i2c flag after the I2C communication is over.
            Settings::isI2CActive = false;
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

            // Fake interrupts may have turned these flags on, set to false.
            Settings::isModeFlagOn = false;
            Settings::isSetFlagOn = false;
        }

        // isThinChar returns true if the value provided is thin character
        // otherwise it returns false.
        const bool isThinChar(int value) const { return (value == 10 || value == 11); }

        // IsNormalMode returns true if the current mode is NORMAL otherwise false.
        const bool IsNormalMode() const { return (m_currentMode == Settings::DisplayMode::NORMAL); }

        // IsSetTimeMode returns true if the current mode is either of SET_TIME modes otherwise false.
        const bool IsSetTimeMode() const { return IsSetSecondsMode() || IsSetMinutesMode() ||  IsSetHoursMode(); }

        // IsSetSecondsMode returns true if the current mode is SET_SECONDS otherwise false.
        const bool IsSetSecondsMode() const { return (m_currentMode == Settings::DisplayMode::SET_SECONDS); }

        // IsSetMinutesMode returns true if the current mode is SET_MINUTES otherwise false.
        const bool IsSetMinutesMode() const { return (m_currentMode == Settings::DisplayMode::SET_MINUTES); }

        // IsSetHoursMode returns true if the current mode is SET_HOURS otherwise false.
        const bool IsSetHoursMode() const { return (m_currentMode == Settings::DisplayMode::SET_HOURS); }

        // isBlink controlls the blinking function of the full colon.
        // Blinking should only happen if the seconds are even and mode is normal.
        const bool isBlink() const { return (Settings::dateTimeArray[0] & 1) && IsNormalMode(); }

        // getLimit returns the max limit set to 24 Hours if mode is SET_HOURS
        // otherwise set to 60 Seconds/Minutes
        const int getLimit() const { return (IsSetHoursMode() ? 24 : 60); };

        // displayTime outputs respective time component on to each display available.
        void displayTime() {

            // Track the display character index position.
            index = 0;

            // Extracts the characters to displayed from the dateTimeArray and
            // formats them based on the matrix character display order.
            for (const auto field : fieldsToDisplay)
            {
                if (index + 1 >= charCount) break;

                // Filter out values greater than mask;
                int digitsToDisplay {Settings::dateTimeArray[field] % digitsMask};
                mappedChars[index] = digitsToDisplay % 10;  // ones position

                // highlight bits are displayed on the last row.
                byte highlight  {(field == m_currentMode) ? 0xF : 0x0};
                setTimeModePos[index++] = highlight;

                mappedChars[index] = (digitsToDisplay / 10) % 10;  // tens position
                setTimeModePos[index++] = highlight;

                // Append colon/blink character between fields if space allows
                if (index < charCount - 1) {
                    mappedChars[index] = (isBlink() ? 10 : 11);
                    setTimeModePos[index++] = 0x0;
                }
            }

            rowData = 0;  // reset the previous data.
            charWidth = 0; // reset the characters width.

            // Draw the mapped characters pixels, one row at a time.
            for (int row{0}; row < Settings::DISPLAY_ROWS; ++row)
            {
                // Reset the index and reuse it below.
                index = 0;
                for (const auto k : mappedChars) {
                    unsigned long charRowPixels{ Settings::display[k][row]};

                    // If one of the SET_TIME modes are active, highlight digits being editted.
                    if (row == (Settings::DISPLAY_ROWS-1))
                        charRowPixels = setTimeModePos[index++]; // Highlight at the last row.

                    // Assign thin char padding if, this char is found or if the
                    // charWith is empty.
                    charWidth += ((isThinChar(k) || charWidth == 0) ? thinCharWidth : normCharWidth);

                    rowData |= charRowPixels << charWidth; // Append the new char bits
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

        // handleMode shifts the current mode to the next inline if an interrupt
        // is detected.
        void handleMode() {
            // Ignore further action if mode flag is off or interrupt if i2c is in progress
            if (!Settings::isModeFlagOn || Settings::isI2CActive) 
                return;

            // Set the interrupt flag to off.
            Settings::isModeFlagOn = false;

            // Reset if current mode is a SET_TIME mode shift by one otherwise set it to seconds.
            m_currentMode = static_cast<Settings::DisplayMode>(
                div(m_currentMode+1, Settings::UNKNOWN).rem
            );

            if (IsNormalMode()) // If NORMAL update the currently set date.
                setDatetime();
        }

        void handleTimeSetting() {
            // Ignore further action if SetFlag is false or current mode is
            // neither any of the SET_TIME modes. Also ignore handling the
            // interrupt if i2c is in progress.
            if (!Settings::isSetFlagOn || !IsSetTimeMode() || Settings::isI2CActive)
                return;

            // Set the interrupt flag to off.
            Settings::isSetFlagOn = false;

            // Increment by one and return to zero if limit is exceeded.
            Settings::dateTimeArray[m_currentMode]++;
            if (Settings::dateTimeArray[m_currentMode] >= getLimit())
                Settings::dateTimeArray[m_currentMode] = (Settings::dateTimeArray[m_currentMode] % getLimit());
            // Serial.println(Settings::dateTimeArray[m_currentMode]);
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
        // Only Seconds, Minutes and Hours should displayed as time.
        const Settings::DisplayMode fieldsToDisplay[3] {
            Settings::DisplayMode::SET_SECONDS,
            Settings::DisplayMode::SET_MINUTES,
            Settings::DisplayMode::SET_HOURS,
        };
        // charCount is set to of 8 because:
        // Hours  => Mapped as 2 Characters
        // Colon => Mapped as 1 Characters
        // Minutes => Mapped as 2 Characters
        // Colon => Mapped as 1 Characters
        // Seconds => Mapped as 2 Characters
        static const int charCount{8};

        // mappedChars holds the DateTime characters for the matrix display.
        int mappedChars[charCount] {0};

        // setTimeModePos holds a boolean value that is set to true if the
        // current mode is set time mode for the specific fields.
        byte setTimeModePos[charCount] {0};

        // displayTime private function variables.
        int charWidth {0};
        int normCharWidth{4}; // Normal characters width = 4
        int thinCharWidth{2}; // Thin characters width = 2.
        unsigned long rowData{0};
        size_t index = 0;
};

unsigned long now;
// handleModeInterrupts set the interrupt Mode flag to true.
void handleModeInterrupts() {
    now = millis();
    if (!Settings::isI2CActive && (now - Settings::lastModeTime > Settings::DEBOUNCE_DELAY)) {
        Settings::isModeFlagOn = true;
        Settings::lastModeTime = now;
    }
}

// handleSetInterrupts sets the interrupt Set flag to true.
void handleSetInterrupts() {
    now = millis();
    if (!Settings::isI2CActive && (now - Settings::lastSetTime > Settings::DEBOUNCE_DELAY)) {
        Settings::isSetFlagOn = true;
        Settings::lastSetTime = now;
    }
}

// Main function.
int main(int, char*) {
    init();

#if defined(USBCON)
    USBDevice.attach();
#endif

    delay(1000); // Delay

    const int devices{4};
    const int DIN{10};
    const int CS{9};
    const int CLK{8};

    // Time setting interrupt pins
    const int MODE_PIN {2};
    const int SET_PIN {3};

    pinMode(DIN, OUTPUT);
    pinMode(CLK, OUTPUT);
    pinMode(CS, OUTPUT);

    pinMode(MODE_PIN, INPUT_PULLUP);
    pinMode(SET_PIN, INPUT_PULLUP);

    // Handle Mode and Set interrupts on falling edge
    attachInterrupt(digitalPinToInterrupt(MODE_PIN), handleModeInterrupts, FALLING);
    attachInterrupt(digitalPinToInterrupt(SET_PIN), handleSetInterrupts, FALLING);

    MatrixDisplay md{DIN, CLK, CS, devices};
    md.setUpDefaults();  // Set display defaults.

    while (true) {
        if(md.IsNormalMode())
            md.readDatetime(); // Reads the current time into dateTimeArray only in NORMAL mode

        md.displayTime();   // Displays the time set in dateTimeArray.
        md.handleMode(); // increment mode.
        md.handleTimeSetting(); // increment time field

        delay(Settings::REFRESH_CYCLE);
    }

    return 0;
}
