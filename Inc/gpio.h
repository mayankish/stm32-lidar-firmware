/* gpio.h -- minimal register-level GPIO helpers (no HAL). */
#ifndef GPIO_H
#define GPIO_H

#include "stm32f4xx.h"

typedef enum { GPIO_MODE_INPUT = 0, GPIO_MODE_OUTPUT = 1, GPIO_MODE_AF = 2, GPIO_MODE_ANALOG = 3 } gpio_mode_t;
typedef enum { GPIO_OTYPE_PP = 0, GPIO_OTYPE_OD = 1 } gpio_otype_t;
typedef enum { GPIO_PUPD_NONE = 0, GPIO_PUPD_UP = 1, GPIO_PUPD_DOWN = 2 } gpio_pupd_t;

/* Enable AHB1 clock for the given port (A/B/C ...) -- call once at boot. */
void gpio_clock_enable(GPIO_TypeDef *port);

void gpio_configure(GPIO_TypeDef *port, uint8_t pin, gpio_mode_t mode,
                     gpio_otype_t otype, gpio_pupd_t pupd, uint8_t af);

static inline void gpio_set(GPIO_TypeDef *port, uint8_t pin)   { port->BSRR = (1u << pin); }
static inline void gpio_clear(GPIO_TypeDef *port, uint8_t pin) { port->BSRR = (1u << (pin + 16)); }
static inline void gpio_toggle(GPIO_TypeDef *port, uint8_t pin) { port->ODR ^= (1u << pin); }
static inline int  gpio_read(GPIO_TypeDef *port, uint8_t pin)  { return (port->IDR >> pin) & 1u; }

#endif /* GPIO_H */
