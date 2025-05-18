#include "mbed.h"

// Define pins connected to the 74HC595 shift register
DigitalOut latchPin(D4);  // ST_CP (latch pin)
DigitalOut clockPin(D7);  // SH_CP (clock pin)
DigitalOut dataPin(D8);   // DS (data pin)

// Define pushbuttons (wired as active LOW)
DigitalIn s1(A1);  // Button to reset time
DigitalIn s2(A2);  // Unused in this code but reserved for future use
DigitalIn s3(A3);  // Button to switch to voltage display

// Analog input from a potentiometer to read voltage
AnalogIn potentiometer(A0);  // Connected to the wiper of the potentiometer

// Segment patterns for common ANODE 7-segment display (inverted logic)
// These values are derived from common cathode patterns but bitwise inverted
const uint8_t digitPattern[10] = {
    static_cast<uint8_t>(~0x3F), // 0
    static_cast<uint8_t>(~0x06), // 1
    static_cast<uint8_t>(~0x5B), // 2
    static_cast<uint8_t>(~0x4F), // 3
    static_cast<uint8_t>(~0x66), // 4
    static_cast<uint8_t>(~0x6D), // 5
    static_cast<uint8_t>(~0x7D), // 6
    static_cast<uint8_t>(~0x07), // 7
    static_cast<uint8_t>(~0x7F), // 8
    static_cast<uint8_t>(~0x6F)  // 9
};

// Bit patterns to select which digit (1 of 4) is active
const uint8_t digitPos[4] = {
    0x01, // Leftmost digit
    0x02, // Second digit from the left
    0x04, // Third digit
    0x08  // Rightmost digit
};

// Global time counters
volatile int seconds = 0;
volatile int minutes = 0;

// Ticker for generating 1-second intervals
Ticker timerTicker;

// Function to update time every second
void updateTime() {
    seconds++;
    if (seconds >= 60) {
        seconds = 0;
        minutes++;
        if (minutes >= 100) minutes = 0; // Wrap around after 99:59
    }
}

// Function to shift out a byte to the shift register (MSB first)
void shiftOutMSBFirst(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        dataPin = (value & (1 << i)) ? 1 : 0;
        clockPin = 1;
        clockPin = 0;
    }
}

// Sends segment data and digit selector to the shift register
void writeToShiftRegister(uint8_t segments, uint8_t digit) {
    latchPin = 0;                       // Start transmission
    shiftOutMSBFirst(segments);        // First send segment pattern
    shiftOutMSBFirst(digit);           // Then select digit position
    latchPin = 1;                       // Latch to output
}

// Function to display a 4-digit number with optional decimal point
void displayNumber(int number, bool showDecimalPoint = false, int decimalPosition = 1) {
    // Split the number into individual digits
    int digits[4] = {
        (number / 1000) % 10,
        (number / 100) % 10,
        (number / 10) % 10,
        number % 10
    };
    
    for (int i = 0; i < 4; i++) {
        uint8_t segments = digitPattern[digits[i]];

        // Add decimal point if requested
        if (showDecimalPoint && i == decimalPosition) {
            segments &= ~0x80;  // Clear bit 7 to enable decimal point (active low)
        }

        writeToShiftRegister(segments, digitPos[i]);
        ThisThread::sleep_for(2ms);  // Small delay to multiplex all digits
    }
}

int main() {
    // Set initial states for shift register pins
    latchPin = 0;
    clockPin = 0;
    dataPin = 0;

    // Enable pull-up resistors on buttons to ensure stable HIGH when unpressed
    s1.mode(PullUp);
    s2.mode(PullUp);
    s3.mode(PullUp);

    // Start the timer to update the time every second
    timerTicker.attach(&updateTime,1.0f);

    while (true) {
        // Check if S1 is pressed to reset time
        if (!s1) {
            seconds = 0;
            minutes = 0;
            ThisThread::sleep_for(200ms);  // Simple debounce delay
        }

        // If S3 is pressed, show the potentiometer voltage instead of time
        if (!s3) {
            float voltage = potentiometer.read_u16() / 65535.0f * 3.3f;  // Convert to 0–3.3V
            int voltageValue = static_cast<int>(voltage * 1000);        // Convert to mV (e.g., 2.75V → 2750)
            displayNumber(voltageValue, true, 0);  // Show as 2.750 with decimal after first digit
        } else {
            // Otherwise, show time in MM:SS format with colon simulated using a decimal point
            int timeValue = minutes * 100 + seconds; // Format: MMSS
            displayNumber(timeValue, true, 1);       // Decimal between minutes and seconds
        }
    }
}

