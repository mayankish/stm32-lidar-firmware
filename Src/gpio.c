#include "gpio.h"

void gpio_clock_enable(GPIO_TypeDef *port) {
    if (port == GPIOA)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    else if (port == GPIOB) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    else if (port == GPIOC) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    __DSB(); /* ensure the clock is on before the peripheral is touched */
}

void gpio_configure(GPIO_TypeDef *port, uint8_t pin, gpio_mode_t mode,
                     gpio_otype_t otype, gpio_pupd_t pupd, uint8_t af) {
    uint32_t pos2 = (uint32_t)pin * 2u;

    port->MODER &= ~(0x3u << pos2);
    port->MODER |= ((uint32_t)mode << pos2);

    port->OTYPER &= ~(0x1u << pin);
    port->OTYPER |= ((uint32_t)otype << pin);

    /* High speed for everything we drive -- stepper timing and UART both
     * benefit, and these are all 3.3V-domain signals well within spec. */
    port->OSPEEDR &= ~(0x3u << pos2);
    port->OSPEEDR |= (0x2u << pos2);

    port->PUPDR &= ~(0x3u << pos2);
    port->PUPDR |= ((uint32_t)pupd << pos2);

    if (mode == GPIO_MODE_AF) {
        if (pin < 8) {
            port->AFR[0] &= ~(0xFu << (pin * 4u));
            port->AFR[0] |= ((uint32_t)af << (pin * 4u));
        } else {
            port->AFR[1] &= ~(0xFu << ((pin - 8) * 4u));
            port->AFR[1] |= ((uint32_t)af << ((pin - 8) * 4u));
        }
    }
}
