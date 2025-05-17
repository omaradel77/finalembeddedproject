#include "mbed.h"

// Shift register pins (common anode)
DigitalOut latchPin(D4);
DigitalOut clockPin(D7);
DigitalOut dataPin(D8);

// Buttons (S1 for reset, S3 for voltage display)
DigitalIn s1(A1), s3(A3);

// ADC for potentiometer (A0)
AnalogIn pot(A0);

// Common anode segment patterns (0-9 with decimal point)
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

// Digit positions (left to right)
const uint8_t digitPos[4] = {0x01, 0x02, 0x04, 0x08};

// Timer and voltage variables
volatile int seconds = 0, minutes = 0;
volatile float minVoltage = 3.3f, maxVoltage = 0.0f;
Ticker timerTicker;

// Function prototypes
void updateTime();
void shiftOutMSBFirst(uint8_t value);
void writeToShiftRegister(uint8_t segments, uint8_t digit);
void displayNumber(int number, bool showDecimal = false, int decimalPos = -1);

// ISR for timer updates
void updateTime() {
    seconds++;
    if (seconds >= 60) {
        seconds = 0;
        minutes = (minutes + 1) % 100;
    }
}

// Shift register driver
void shiftOutMSBFirst(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        dataPin = (value & (1 << i)) ? 1 : 0;
        clockPin = 1;
        clockPin = 0;
    }
}

// Write data to shift register
void writeToShiftRegister(uint8_t segments, uint8_t digit) {
    latchPin = 0;
    shiftOutMSBFirst(segments);
    shiftOutMSBFirst(digit);
    latchPin = 1;
}

// Display number with optional decimal point
void displayNumber(int number, bool showDecimal, int decimalPos) {
    int digits[4] = {
        (number / 1000) % 10,
        (number / 100) % 10,
        (number / 10) % 10,
        number % 10
    };

    for (int i = 0; i < 4; i++) {
        uint8_t pattern = digitPattern[digits[i]];
        if (showDecimal && i == decimalPos) pattern &= ~0x80; // Enable DP
        writeToShiftRegister(pattern, digitPos[i]);
        ThisThread::sleep_for(2);
    }
}

int main() {
    // Initialize hardware
    s1.mode(PullUp);
    s3.mode(PullUp);
    timerTicker.attach(&updateTime, 1.0f); // 1-second updates

    while (1) {
        // Reset timer on S1 press
        if (!s1) {
            seconds = 0;
            minutes = 0;
            ThisThread::sleep_for(200); // Debounce
        }

        // Read potentiometer voltage
        float voltage = pot.read() * 3.3f; // Convert to volts (0-3.3V)

        // Update min/max voltages
        if (voltage < minVoltage) minVoltage = voltage;
        if (voltage > maxVoltage) maxVoltage = voltage;

        // Display voltage if S3 pressed, else display time
        if (!s3) {
            int voltageScaled = static_cast<int>(voltage * 100); // e.g., 2.45V → 245
            displayNumber(voltageScaled, true, 1); // Show as X.XX
        } else {
            int timeDisplay = minutes * 100 + seconds; // MMSS format
            displayNumber(timeDisplay);
        }
    }
}