#include "stm32f4xx.h"
#include <stdio.h>

/* --- GLOBAL VARIABLES (Project Requirements) --- */
volatile uint32_t system_ms = 0;              // Timestamp (Time since boot)
volatile uint32_t event_counter = 0;          // Total number of detected events
volatile uint32_t last_event_duration = 0;    // Duration of the most recent event

volatile uint16_t adc_max = 0;
volatile uint16_t adc_min = 4095;
volatile uint16_t sample_counter = 0;

/* State Machine Variables */
volatile uint8_t is_event_active = 0;         // Status flag for an ongoing event
volatile uint32_t event_start_time = 0;       // Millisecond mark when event started
volatile uint16_t peak_amplitude = 0;         // Highest intensity recorded during event
volatile uint16_t locked_threshold = 0;       // Threshold value captured at trigger
volatile uint8_t silence_timer = 0;           // Timer to detect end of sound (debounce)
volatile uint8_t print_pending_flag = 0;      // Flag to signal main loop for UART output

/* --- ADC FUNCTIONS --- */
void ADC1_Init(void)
{
    RCC->APB2ENR |= (1 << 8); // Enable ADC1 Clock
    ADC1->CR2 |= (1 << 0);    // Power on ADC
}

uint16_t ADC1_Get_Value(uint8_t channel)
{
    ADC1->SQR3 = channel;           // Select Channel (0: Mic, 1: Pot)
    ADC1->CR2 |= (1 << 30);         // Start Conversion
    while (!(ADC1->SR & (1 << 1))); // Wait for End of Conversion
    return ADC1->DR;
}

/* --- UART FUNCTIONS --- */
void USART2_Init(void)
{
    RCC->APB1ENR |= (1 << 17);      // Enable USART2 Clock
    RCC->AHB1ENR |= (1 << 0);       // Enable GPIOA Clock

    // Configure PA2 (TX) and PA3 (RX) as Alternate Function
    GPIOA->MODER &= ~(0xF << 4);
    GPIOA->MODER |= (0xA << 4);
    GPIOA->AFR[0] |= (0x77 << 8);   // AF7 for USART2

    USART2->BRR = 0x0683;           // 9600 Baud Rate @ 16MHz
    USART2->CR1 |= (1 << 13) | (1 << 3) | (1 << 2); // UE, TE, RE enabled
}

void UART_Print(char* string)
{
    while (*string)
    {
        while (!(USART2->SR & (1 << 7))); // Wait until TXE is high
        USART2->DR = *string++;
    }
}

/* --- SYSTICK INTERRUPT --- */
void SysTick_Handler(void)
{
    system_ms++; // Increment global time every 1ms
}

/* --- MAIN PROGRAM --- */
int main() {
    ADC1_Init();
    USART2_Init();

    // Configure GPIOs
    GPIOA->MODER |= (0xF << 0);     // PA0 and PA1: Analog Mode
    GPIOA->MODER &= ~(3 << 8);
    GPIOA->MODER |= (1 << 10);       // PA5: Output Mode (LED)

    /* --- Bare-Metal SysTick Configuration --- */
    // 1. Load the value for 1ms interval (16MHz / 1000 = 16000 cycles)
    SysTick->LOAD = (16000 - 1);

    // 2. Clear current value register
    SysTick->VAL = 0;

    // 3. Control Register Settings:
    // Bit 2 (CLKSOURCE): 1 -> Processor Clock (AHB)
    // Bit 1 (TICKINT):   1 -> Enable Interrupt
    // Bit 0 (ENABLE):    1 -> Enable SysTick
    SysTick->CTRL = (1 << 2) | (1 << 1) | (1 << 0);          // Setup SysTick for 1ms interval

    // TIM2 Configuration (100us sampling period)
    RCC->APB1ENR |= (1 << 0);       // Enable TIM2 Clock
    TIM2->PSC = 16 - 1;             // 1 MHz resolution
    TIM2->ARR = 100 - 1;            // 100 us period
    TIM2->DIER |= (1 << 0);         // Enable update interrupt
    NVIC->ISER[0] |= (1 << 28);     // Enable TIM2 IRQ in NVIC
    TIM2->CR1 |= (1 << 0);          // Start Timer

    char log_buffer[128];
    UART_Print("\r\n--- MONITORING ACTIVE. WAITING FOR TRIGGER... ---\r\n");

    while(1)
    {
        if (print_pending_flag == 1)
        {
            // Log format: Timestamp | Event # | Peak Amplitude | Current Threshold | Duration
            sprintf(log_buffer, "[Time: %lu ms] Event #%lu | Peak: %d | Threshold: %d | Dur: %lu ms\r\n",
                    system_ms, event_counter, peak_amplitude, locked_threshold, last_event_duration);

            UART_Print(log_buffer);
            print_pending_flag = 0;
        }
    }
}

/* --- TIMER INTERRUPT (DATA ACQUISITION) --- */
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & (1 << 0))
    {
        TIM2->SR &= ~(1 << 0); // Clear Interrupt Flag

        // Read Microphone (PA0)
        uint16_t current_reading = ADC1_Get_Value(0);
        if (current_reading > adc_max) adc_max = current_reading;
        if (current_reading < adc_min) adc_min = current_reading;

        sample_counter++;

        // End of 50ms Window (500 samples * 100us = 50ms)
        if (sample_counter >= 500)
        {
            uint16_t current_amplitude = adc_max - adc_min;
            uint16_t pot_value = ADC1_Get_Value(1); // Read Potentiometer (PA1)

            // Noise floor filter: Minimum threshold set to 50
            if (pot_value < 50) pot_value = 50;

            if (current_amplitude > pot_value)
            {
                if (is_event_active == 0)
                {
                    // Start of a new sound event
                    is_event_active = 1;
                    event_start_time = system_ms;
                    peak_amplitude = current_amplitude;
                    locked_threshold = pot_value; // Lock the current threshold for reporting
                    GPIOA->ODR |= (1 << 5);       // Trigger LED
                }
                else if (current_amplitude > peak_amplitude)
                {
                    // Update peak if the sound gets louder
                    peak_amplitude = current_amplitude;
                }
                silence_timer = 0; // Reset silence detection
            }
            else if (is_event_active == 1)
            {
                // Sound below threshold, check if event has finished
                silence_timer++;
                if (silence_timer >= 5)
                { // 5 windows (250ms) of silence needed
                    is_event_active = 0;
                    last_event_duration = system_ms - event_start_time;
                    event_counter++;
                    print_pending_flag = 1;
                    GPIOA->ODR &= ~(1 << 5);      // Turn off LED
                }
            }

            // Reset for the next 50ms window
            sample_counter = 0;
            adc_max = 0;
            adc_min = 4095;
        }
    }
}
