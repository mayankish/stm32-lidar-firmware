/*
 * syscalls.c -- minimal newlib-nano syscall stubs.
 *
 * We link with -specs=nano.specs -specs=nosys.specs (see Makefile), which
 * already provides no-op stubs for everything below. This file exists
 * only to retarget _write() to USART2 (the ST-Link VCP debug UART), so
 * that printf()/puts() during development are useful instead of being
 * silently swallowed by the nosys stub. Nothing else in the firmware
 * relies on libc I/O -- the data-contract link (USART1) never goes
 * through stdio, by design (see uart.c: uart1_write is called directly).
 */
#include <sys/stat.h>
#include <errno.h>
#include "stm32f4xx.h"
#include "uart.h"

int _write(int file, char *ptr, int len) {
    if (file != 1 && file != 2) { /* only stdout/stderr */
        errno = EBADF;
        return -1;
    }
    uart2_write((const uint8_t *)ptr, (size_t)len);
    return len;
}

int _close(int file) { (void)file; return -1; }
int _fstat(int file, struct stat *st) { (void)file; st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { (void)file; return 1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
