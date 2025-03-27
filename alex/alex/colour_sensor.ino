#include <avr/io.h>
#include <util/delay.h>

#define S0_PIN PK0  // S0 -> A8
#define S1_PIN PK1  // S1 -> A9
#define S2_PIN PK2  // S2 -> A10
#define S3_PIN PK3  // S3 -> A11
#define OUT_PIN PK4  // OUT -> A12


void GPIO_init() {
    DDRK |= (1 << S0_PIN) | (1 << S1_PIN) | (1 << S2_PIN) | (1 << S3_PIN); // Set control pins as OUTPUT
    DDRK &= ~(1 << OUT_PIN); // OUT_PIN as INPUT

    PORTK |= (1 << S0_PIN); // S0 HIGH (Frequency Scaling 100%)
    PORTK |= (1 << S1_PIN); // S1 HIGH
}

uint16_t GetPulseUsingTimer() {
    TCNT1 = 0;                          // Reset Timer1 counter
    TCCR1B = (1 << CS11);               // Start Timer1 with prescaler 8

    while (!(PINK & (1 << OUT_PIN)));   // Wait for rising edge (HIGH)
    TCNT1 = 0;                          // Reset counter at the rising edge

    while (PINK & (1 << OUT_PIN));      // Wait for falling edge (LOW)
    TCCR1B = 0;                         // Stop Timer1

    return TCNT1;  // Return pulse width (higher value = lower frequency)
}

void GetColors() {
    // RED
    PORTK &= ~(1 << S2_PIN); PORTK &= ~(1 << S3_PIN);
    _delay_ms(20);
    Red = GetPulseUsingTimer();

    // BLUE
    PORTK &= ~(1 << S2_PIN); PORTK |= (1 << S3_PIN);
    _delay_ms(20);
    Blue = GetPulseUsingTimer();

    // GREEN
    PORTK |= (1 << S2_PIN); PORTK |= (1 << S3_PIN);
    _delay_ms(20);
    Green = GetPulseUsingTimer();
}


