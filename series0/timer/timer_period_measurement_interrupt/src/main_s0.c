/**************************************************************************//**
 * @main_series0.c
 * @brief This project demonstrates period measurement using input capture.
 * A periodic input signal is routed to a Compare/Capture channel, and each
 * period length is calculated from the captured edges. Connect a periodic
 * signal to the GPIO pin specified in the readme.txt input. Note maximum
 * measurable frequency is 333 kHz.
 * @version 0.0.1
 ******************************************************************************
 * @section License
 * <b>Copyright 2018 Silicon Labs, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "em_device.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_chip.h"
#include "em_gpio.h"
#include "em_timer.h"

// Default prescale value
#define TIMER1_PRESCALE timerPrescale1

// Default clock value
#define HFPERCLK_IN_MHZ 14

// Most recent measured period in microseconds
static volatile uint32_t measuredPeriod;

// Stored edge from previous interrupt
static volatile uint32_t lastCapturedEdge;

// Number of timer overflows since last interrupt;
static volatile uint32_t overflowCount;

/**************************************************************************//**
 * @brief
 *    Interrupt handler for TIMER1
 *
 * @note
 *    The first interrupt won't produce the correct measuredPeriod value because
 *    lastCapturedEdge will be some random value.
 *****************************************************************************/
void TIMER1_IRQHandler(void)
{
  // Acknowledge the interrupt
  uint32_t flags = TIMER_IntGet(TIMER1);
  TIMER_IntClear(TIMER1, flags);

  // Read the capture value from the CC register
  uint32_t current_edge = TIMER_CaptureGet(TIMER1, 0);

  // Check if the timer overflowed
  if (flags & TIMER_IF_OF) {
    overflowCount++;
  }

  // Check if a capture event occurred
  if (flags & TIMER_IF_CC0) {

    // Calculate period in microseconds, while compensating for overflows
    // Interrupt latency will affect measurements for periods below 3 microseconds (333 kHz)
    measuredPeriod = (overflowCount * (TIMER_TopGet(TIMER1) + 2)
                      - lastCapturedEdge + current_edge)
                      / (HFPERCLK_IN_MHZ * (1 << TIMER1_PRESCALE));

    // Record the capture value for the next period measurement calculation
    lastCapturedEdge = current_edge;

    // Reset the overflow count
    overflowCount = 0;
  }
}

/**************************************************************************//**
 * @brief
 *    GPIO initialization
 *****************************************************************************/
void initGpio(void)
{
  // Enable GPIO clock
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Configure TIMER1 CC0 Location 4 (PD6) as input
  GPIO_PinModeSet(gpioPortD, 6, gpioModeInput, 0);
}

/**************************************************************************//**
 * @brief
 *    TIMER initialization
 *****************************************************************************/
void initTimer(void)
{
  // Enable clock for TIMER1 module
  CMU_ClockEnable(cmuClock_TIMER1, true);

  // Configure the TIMER1 module for Capture mode and to trigger on falling edges
  TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;
  timerCCInit.edge = timerEdgeFalling;
  timerCCInit.mode = timerCCModeCapture;
  TIMER_InitCC(TIMER1, 0, &timerCCInit);

  // Route TIMER1 CC0 to location 4 and enable CC0 route pin
  // TIM1_CC0 #4 is GPIO Pin PD6
  TIMER1->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC4);

  // Initialize timer with defined prescale value
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
  timerInit.prescale = TIMER1_PRESCALE;
  TIMER_Init(TIMER1, &timerInit);

  // Enable TIMER1 interrupts
  TIMER_IntEnable(TIMER1, TIMER_IEN_CC0 | TIMER_IEN_OF);
  NVIC_EnableIRQ(TIMER1_IRQn);
}

/**************************************************************************//**
 * @brief
 *    Main function
 *****************************************************************************/
int main(void)
{
  // Chip errata
  CHIP_Init();

  // Initializations
  initGpio();
  initTimer();

  while (1) {
    EMU_EnterEM1(); // Enter EM1 (won't exit)
  }
}

