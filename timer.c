#include <avr/io.h>
#include <util/atomic.h>

#include <timer.h>

PRIVATE volatile uint64_t counter;

PUBLIC bool initMSTimer(void) {
    uint16_t OCRA = 0U;

    // CTC on OCR1A - WGM3:0 - 4
    TCCR1B |= _BV(WGM12);

    // Clock selection - prescaler 64
    TCCR1B |= _BV(CS11) | _BV(CS10);

    // Enable interrupt on compare with OCCR1A
    TIMSK1 |= _BV(OCIE1A);

    // Calculate for 1 ms interrupt
    OCRA = (F_CPU / 64) / 1000;

    OCR1AH = (OCRA >> 8) & 0xff;
    OCR1AL = OCRA & 0xff;

    return TRUE;
}

PUBLIC uint64_t timer_read_counter(void) {
    uint64_t temp;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        temp = counter;
    }

    return temp;
}

PUBLIC void delay_ms(uint64_t ms_count) {
    uint64_t base = timer_read_counter();

    while ((timer_read_counter() - base) < ms_count) {
        // Loop here
        ;
    }
}

ISR(TIMER1_COMPA_vect) {
    // Increase the counter
    counter++;
}