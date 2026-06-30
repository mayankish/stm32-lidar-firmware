    .syntax unified
    .cpu cortex-m4
    .fpu softvfp
    .thumb

.global  Default_Handler
.global  g_pfnVectors

/* ---- start-of-data symbols supplied by the linker script ---- */
.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

/* ============================ Reset_Handler ============================ */
.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  ldr   sp, =_estack        /* set stack pointer */

  /* copy .data from flash to RAM */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b CopyDataInit

CopyDataLoop:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

CopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataLoop

  /* zero .bss */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str  r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

  /* C library / system / kernel init, then main() */
  bl  SystemInit
  bl  __libc_init_array
  bl  main
  bx  lr   /* should never return, but be tidy */

.size Reset_Handler, .-Reset_Handler

/* ============================ Default_Handler ============================
 * Infinite loop for any unhandled exception/IRQ -- deliberately a trap,
 * not a silent ignore, so a stray interrupt is obvious on a debugger. */
.section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b  Infinite_Loop
.size Default_Handler, .-Default_Handler

/* ============================ Vector table ============================ */
.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
.size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word vPortSVCHandler        /* SVCall   -- aliased to FreeRTOS port  */
  .word DebugMon_Handler
  .word 0
  .word xPortPendSVHandler     /* PendSV   -- aliased to FreeRTOS port  */
  .word xPortSysTickHandler    /* SysTick  -- aliased to FreeRTOS port  */

  /* ---- external interrupts (RM0383 Table 38) ---- */
  .word WWDG_IRQHandler             /* 0  */
  .word PVD_IRQHandler               /* 1  */
  .word TAMP_STAMP_IRQHandler        /* 2  */
  .word RTC_WKUP_IRQHandler          /* 3  */
  .word FLASH_IRQHandler             /* 4  */
  .word RCC_IRQHandler                /* 5  */
  .word EXTI0_IRQHandler              /* 6  */
  .word EXTI1_IRQHandler              /* 7  */
  .word EXTI2_IRQHandler              /* 8  */
  .word EXTI3_IRQHandler              /* 9  */
  .word EXTI4_IRQHandler              /* 10 */
  .word DMA1_Stream0_IRQHandler       /* 11 */
  .word DMA1_Stream1_IRQHandler       /* 12 */
  .word DMA1_Stream2_IRQHandler       /* 13 */
  .word DMA1_Stream3_IRQHandler       /* 14 */
  .word DMA1_Stream4_IRQHandler       /* 15 */
  .word DMA1_Stream5_IRQHandler       /* 16 */
  .word DMA1_Stream6_IRQHandler       /* 17 */
  .word ADC_IRQHandler                /* 18 */
  .word 0                              /* 19 reserved */
  .word 0                              /* 20 reserved */
  .word 0                              /* 21 reserved */
  .word 0                              /* 22 reserved */
  .word EXTI9_5_IRQHandler            /* 23 */
  .word TIM1_BRK_TIM9_IRQHandler      /* 24 */
  .word TIM1_UP_TIM10_IRQHandler      /* 25 */
  .word TIM1_TRG_COM_TIM11_IRQHandler /* 26 */
  .word TIM1_CC_IRQHandler            /* 27 */
  .word TIM2_IRQHandler               /* 28 -- used here for stepper step timing */
  .word TIM3_IRQHandler               /* 29 */
  .word TIM4_IRQHandler               /* 30 */
  .word I2C1_EV_IRQHandler            /* 31 -- VL53L0X bus */
  .word I2C1_ER_IRQHandler            /* 32 */
  .word I2C2_EV_IRQHandler            /* 33 */
  .word I2C2_ER_IRQHandler            /* 34 */
  .word SPI1_IRQHandler                /* 35 */
  .word SPI2_IRQHandler                /* 36 */
  .word USART1_IRQHandler              /* 37 -- ESP32 link, RX drives command_handler */
  .word USART2_IRQHandler              /* 38 -- ST-Link VCP debug printf */
  .word 0                               /* 39 reserved */
  .word EXTI15_10_IRQHandler            /* 40 */
  .word RTC_Alarm_IRQHandler            /* 41 */
  .word OTG_FS_WKUP_IRQHandler          /* 42 */
  .word 0
  .word 0
  .word 0
  .word 0
  .word DMA1_Stream7_IRQHandler         /* 47 */
  .word 0
  .word SDIO_IRQHandler                 /* 49 */
  .word TIM5_IRQHandler                 /* 50 */
  .word SPI3_IRQHandler                 /* 51 */
  .word 0
  .word 0
  .word 0
  .word 0
  .word DMA2_Stream0_IRQHandler         /* 56 */
  .word DMA2_Stream1_IRQHandler         /* 57 */
  .word DMA2_Stream2_IRQHandler         /* 58 */
  .word DMA2_Stream3_IRQHandler         /* 59 */
  .word DMA2_Stream4_IRQHandler         /* 60 */
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word OTG_FS_IRQHandler               /* 67 */
  .word DMA2_Stream5_IRQHandler         /* 68 */
  .word DMA2_Stream6_IRQHandler         /* 69 */
  .word DMA2_Stream7_IRQHandler         /* 70 */
  .word USART6_IRQHandler               /* 71 */
  .word I2C3_EV_IRQHandler              /* 72 */
  .word I2C3_ER_IRQHandler              /* 73 */
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word FPU_IRQHandler                  /* 81 */
  .word 0
  .word 0
  .word SPI4_IRQHandler                 /* 84 */
  .word SPI5_IRQHandler                 /* 85 */

/* ---- weak aliases: any handler not implemented elsewhere falls back to
 *      Default_Handler (a trap loop), never to undefined behaviour. ---- */
.weak NMI_Handler
.thumb_set NMI_Handler,Default_Handler
.weak HardFault_Handler
.thumb_set HardFault_Handler,Default_Handler
.weak MemManage_Handler
.thumb_set MemManage_Handler,Default_Handler
.weak BusFault_Handler
.thumb_set BusFault_Handler,Default_Handler
.weak UsageFault_Handler
.thumb_set UsageFault_Handler,Default_Handler
.weak DebugMon_Handler
.thumb_set DebugMon_Handler,Default_Handler

.weak WWDG_IRQHandler
.thumb_set WWDG_IRQHandler,Default_Handler
.weak PVD_IRQHandler
.thumb_set PVD_IRQHandler,Default_Handler
.weak TAMP_STAMP_IRQHandler
.thumb_set TAMP_STAMP_IRQHandler,Default_Handler
.weak RTC_WKUP_IRQHandler
.thumb_set RTC_WKUP_IRQHandler,Default_Handler
.weak FLASH_IRQHandler
.thumb_set FLASH_IRQHandler,Default_Handler
.weak RCC_IRQHandler
.thumb_set RCC_IRQHandler,Default_Handler
.weak EXTI0_IRQHandler
.thumb_set EXTI0_IRQHandler,Default_Handler
.weak EXTI1_IRQHandler
.thumb_set EXTI1_IRQHandler,Default_Handler
.weak EXTI2_IRQHandler
.thumb_set EXTI2_IRQHandler,Default_Handler
.weak EXTI3_IRQHandler
.thumb_set EXTI3_IRQHandler,Default_Handler
.weak EXTI4_IRQHandler
.thumb_set EXTI4_IRQHandler,Default_Handler
.weak DMA1_Stream0_IRQHandler
.thumb_set DMA1_Stream0_IRQHandler,Default_Handler
.weak DMA1_Stream1_IRQHandler
.thumb_set DMA1_Stream1_IRQHandler,Default_Handler
.weak DMA1_Stream2_IRQHandler
.thumb_set DMA1_Stream2_IRQHandler,Default_Handler
.weak DMA1_Stream3_IRQHandler
.thumb_set DMA1_Stream3_IRQHandler,Default_Handler
.weak DMA1_Stream4_IRQHandler
.thumb_set DMA1_Stream4_IRQHandler,Default_Handler
.weak DMA1_Stream5_IRQHandler
.thumb_set DMA1_Stream5_IRQHandler,Default_Handler
.weak DMA1_Stream6_IRQHandler
.thumb_set DMA1_Stream6_IRQHandler,Default_Handler
.weak ADC_IRQHandler
.thumb_set ADC_IRQHandler,Default_Handler
.weak EXTI9_5_IRQHandler
.thumb_set EXTI9_5_IRQHandler,Default_Handler
.weak TIM1_BRK_TIM9_IRQHandler
.thumb_set TIM1_BRK_TIM9_IRQHandler,Default_Handler
.weak TIM1_UP_TIM10_IRQHandler
.thumb_set TIM1_UP_TIM10_IRQHandler,Default_Handler
.weak TIM1_TRG_COM_TIM11_IRQHandler
.thumb_set TIM1_TRG_COM_TIM11_IRQHandler,Default_Handler
.weak TIM1_CC_IRQHandler
.thumb_set TIM1_CC_IRQHandler,Default_Handler
.weak TIM2_IRQHandler
.thumb_set TIM2_IRQHandler,Default_Handler
.weak TIM3_IRQHandler
.thumb_set TIM3_IRQHandler,Default_Handler
.weak TIM4_IRQHandler
.thumb_set TIM4_IRQHandler,Default_Handler
.weak I2C1_EV_IRQHandler
.thumb_set I2C1_EV_IRQHandler,Default_Handler
.weak I2C1_ER_IRQHandler
.thumb_set I2C1_ER_IRQHandler,Default_Handler
.weak I2C2_EV_IRQHandler
.thumb_set I2C2_EV_IRQHandler,Default_Handler
.weak I2C2_ER_IRQHandler
.thumb_set I2C2_ER_IRQHandler,Default_Handler
.weak SPI1_IRQHandler
.thumb_set SPI1_IRQHandler,Default_Handler
.weak SPI2_IRQHandler
.thumb_set SPI2_IRQHandler,Default_Handler
.weak USART1_IRQHandler
.thumb_set USART1_IRQHandler,Default_Handler
.weak USART2_IRQHandler
.thumb_set USART2_IRQHandler,Default_Handler
.weak EXTI15_10_IRQHandler
.thumb_set EXTI15_10_IRQHandler,Default_Handler
.weak RTC_Alarm_IRQHandler
.thumb_set RTC_Alarm_IRQHandler,Default_Handler
.weak OTG_FS_WKUP_IRQHandler
.thumb_set OTG_FS_WKUP_IRQHandler,Default_Handler
.weak DMA1_Stream7_IRQHandler
.thumb_set DMA1_Stream7_IRQHandler,Default_Handler
.weak SDIO_IRQHandler
.thumb_set SDIO_IRQHandler,Default_Handler
.weak TIM5_IRQHandler
.thumb_set TIM5_IRQHandler,Default_Handler
.weak SPI3_IRQHandler
.thumb_set SPI3_IRQHandler,Default_Handler
.weak DMA2_Stream0_IRQHandler
.thumb_set DMA2_Stream0_IRQHandler,Default_Handler
.weak DMA2_Stream1_IRQHandler
.thumb_set DMA2_Stream1_IRQHandler,Default_Handler
.weak DMA2_Stream2_IRQHandler
.thumb_set DMA2_Stream2_IRQHandler,Default_Handler
.weak DMA2_Stream3_IRQHandler
.thumb_set DMA2_Stream3_IRQHandler,Default_Handler
.weak DMA2_Stream4_IRQHandler
.thumb_set DMA2_Stream4_IRQHandler,Default_Handler
.weak OTG_FS_IRQHandler
.thumb_set OTG_FS_IRQHandler,Default_Handler
.weak DMA2_Stream5_IRQHandler
.thumb_set DMA2_Stream5_IRQHandler,Default_Handler
.weak DMA2_Stream6_IRQHandler
.thumb_set DMA2_Stream6_IRQHandler,Default_Handler
.weak DMA2_Stream7_IRQHandler
.thumb_set DMA2_Stream7_IRQHandler,Default_Handler
.weak USART6_IRQHandler
.thumb_set USART6_IRQHandler,Default_Handler
.weak I2C3_EV_IRQHandler
.thumb_set I2C3_EV_IRQHandler,Default_Handler
.weak I2C3_ER_IRQHandler
.thumb_set I2C3_ER_IRQHandler,Default_Handler
.weak FPU_IRQHandler
.thumb_set FPU_IRQHandler,Default_Handler
.weak SPI4_IRQHandler
.thumb_set SPI4_IRQHandler,Default_Handler
.weak SPI5_IRQHandler
.thumb_set SPI5_IRQHandler,Default_Handler
