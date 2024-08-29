#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include "avr_common/uart.h"

#define MAX_EVENTS      12
#define DEBOUNCE_DELAY  5   // 5ms debounce delay

// MIDI Note ON/OFF message constants
#define MIDI_NOTE_ON    0x90
#define MIDI_NOTE_OFF   0x80
#define BASE_MIDI_NOTE  60  // C3

// Structure to represent a key event
typedef struct {
    uint8_t status: 1;  // 1 = pressed, 0 = released
    uint8_t key: 7; // key number (0 to 15)
} KeyEvent;

uint16_t key_status = 0;    // Current key status

// Function to scan the keyboard for 12 keys across PORTA and PORTC
uint8_t keyScan(KeyEvent* events) {
    uint16_t new_status = 0;    // New key status
    int num_events = 0; // Number of events

    // Scan keys on PORTA (first 8 keys)
    for (uint8_t key_num = 0; key_num < 8; ++key_num) {
        uint16_t key_mask = 1 << key_num;
        uint8_t pin_state = PINA & (1 << key_num);  // Read the state of the pin for each key on PORTA

        uint8_t cs = (pin_state == 0);  // 1 if key pressed (active low)

        if (cs) {
            new_status |= key_mask; // Update new key status
        }

        uint8_t ps = (key_mask & key_status) != 0;  // Previous key status

        // If the key status has changed, register an event
        if (cs != ps) {
            KeyEvent e;
            e.key = key_num;
            e.status = cs;
            events[num_events] = e;
            ++num_events;
        }
    }

    // Scan keys on PORTC (next 4 keys)
    for (uint8_t key_num = 8; key_num < 12; ++key_num) {
        uint16_t key_mask = 1 << key_num;
        uint8_t pin_state = PINC & (1 << (key_num - 8));    // Read the state of the pin for each key on PORTC

        uint8_t cs = (pin_state == 0);  // 1 if key pressed (active low)

        if (cs) {
            new_status |= key_mask; // Update new key status
        }

        uint8_t ps = (key_mask & key_status) != 0;  // Previous key status

        // If the key status has changed, register an event
        if (cs != ps) {
            KeyEvent e;
            e.key = key_num;
            e.status = cs;
            events[num_events] = e;
            ++num_events;
        }
    }

    key_status = new_status;    // Update global key status
    return num_events;  // Return number of events
}

// Send MIDI message over UART
void send_midi(uint8_t status, uint8_t note, uint8_t velocity) {
    usart_putchar(status);  // Send Note ON/OFF
    usart_putchar(note);    // Send the Note value
    usart_putchar(velocity);    // Send the velocity
}

int main(void) {
    // Initialize UART for MIDI communication
    printf_init();

    DDRA = 0x00;    // Set all bits of PORTA as input
    PORTA = 0xFF;   // Enable pull-up resistors on all pins of PORTA

    DDRC = 0x00;    // Set all bits of PORTC as input
    PORTC = 0xFF;   // Enable pull-up resistors on all pins of PORTC

    KeyEvent events[MAX_EVENTS];

    while (1) {
        uint8_t num_events = keyScan(events);   // Scan the keyboard for events
        for (uint8_t k = 0; k < num_events; ++k) {
            KeyEvent e = events[k];
            uint8_t note = BASE_MIDI_NOTE + e.key;  // Map key number to MIDI note
            if (e.status == 1) {
                send_midi(MIDI_NOTE_ON, note, 127); // Note ON, velocity 127
                // printf("ON: [%02X %02X %02X]\n", MIDI_NOTE_ON, note, 127);
            }
            else {
                send_midi(MIDI_NOTE_OFF, note, 0);  // Note OFF, velocity 0
                // printf("OFF: [%02X %02X %02X]\n", MIDI_NOTE_OFF, note, 0);
            }
        }
    }
}
