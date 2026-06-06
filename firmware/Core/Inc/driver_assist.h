/**
 * driver_assist.h
 * Driver Assistance -- Obstacle Warning & Brake Alert System
 * Target : STM32 Nucleo-F401RE
 * Course : EE325 Embedded Systems, Spring 2025-2026
 */
#ifndef DRIVER_ASSIST_H
#define DRIVER_ASSIST_H

/* Call once from main(), after MX_ADC1_Init() and MX_GPIO_Init().
 * Contains the infinite while(1) loop -- never returns. */
void driver_assist_run(void);

#endif /* DRIVER_ASSIST_H */
