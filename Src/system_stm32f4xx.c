/*
 * system_stm32f4xx.c -- minimal clock/system init, hand-written (NOT the
 * ST-generated CMSIS system file, which pulls in a lot of unused
 * configurability). Brings the STM32F411RE up from the internal 16MHz HSI
 * to 84MHz via the main PLL, since this project targets bare-metal/no-HAL
 * and the Nucleo board's HSE (8MHz from ST-Link MCO) is not guaranteed
 * stable enough to depend on without extra bring-up code -- HSI+PLL is
 * the simpler, fully self-contained choice and is documented as such in
 * the README "Known limitations" section.
 *
 * PLL math for 84MHz SYSCLK from 16MHz HSI:
 *   VCO_in  = HSI / PLLM = 16MHz / 16   = 1MHz   (must be 1-2MHz, RM0383)
 *   VCO_out = VCO_in * PLLN = 1MHz * 336 = 336MHz (must be 192-432MHz)
 *   SYSCLK  = VCO_out / PLLP = 336MHz / 4 = 84MHz
 *   USB/SDIO (unused here) = VCO_out / PLLQ = 336MHz / 7 = 48MHz
 *
 * AHB prescaler /1 -> HCLK = 84MHz
 * APB1 prescaler /2 -> PCLK1 = 42MHz (max 42MHz on F411 -- exact fit)
 * APB2 prescaler /1 -> PCLK2 = 84MHz (max 100MHz on F411 -- fine)
 */
#include "stm32f4xx.h"

uint32_t SystemCoreClock = 84000000UL;

static void SystemClock_Config(void) {
    /* 1. Enable HSI (already on by default at reset, but be explicit) and
     *    wait for it to be ready. */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) { /* wait */ }

    /* 2. Configure Flash prefetch + wait states for 84MHz @ 3.3V (RM0383
     *    Table 10: 2 wait states covers up to 90MHz). */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;

    /* 3. Configure the main PLL (source = HSI), but PLL must be off while
     *    reconfiguring. */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) { /* wait for it to actually stop */ }

    RCC->PLLCFGR =
        (16u << RCC_PLLCFGR_PLLM_Pos) |
        (336u << RCC_PLLCFGR_PLLN_Pos) |
        (1u << RCC_PLLCFGR_PLLP_Pos) | /* 0b01 -> /4, see RM0383 PLLP encoding */
        (0u << RCC_PLLCFGR_PLLSRC_Pos) | /* 0 = HSI */
        (7u << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { /* wait */ }

    /* 4. Bus prescalers: AHB /1, APB1 /2, APB2 /1 -- set BEFORE switching
     *    SYSCLK source so APB1 is never briefly out of spec. */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2; /* APB1 = HCLK/2 = 42MHz */
    /* HPRE=0 (/1), PPRE2=0 (/1) already correct after the clear above. */

    /* 5. Switch SYSCLK to the PLL and wait for confirmation. */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { /* wait */ }
}

void SystemInit(void) {
    /* Called from Reset_Handler before .data/.bss init per the standard
     * CMSIS contract, but our clock switch touches only registers (no
     * globals), so doing it here vs. after .bss-zero makes no difference;
     * kept here to match the conventional CMSIS startup contract that the
     * vendored CMSIS-Device-F4 headers assume. */
    SystemClock_Config();
}
