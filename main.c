/* main.c - Firmware TRNG Autónomo en C Puro para Puya PY32F002A */
#include <stdint.h>

/* Direcciones de Registros Base del PY32F002A */
#define RCC_BASE        0x40021000
#define RCC_IOPENR      (*(volatile uint32_t*)(RCC_BASE + 0x34))
#define RCC_APB2ENR     (*(volatile uint32_t*)(RCC_BASE + 0x30))
#define RCC_CSR         (*(volatile uint32_t*)(RCC_BASE + 0x24))

#define GPIOA_BASE      0x40010800
#define GPIOA_MODER     (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL      (*(volatile uint32_t*)(GPIOA_BASE + 0x20))

#define USART1_BASE     0x40013800
#define USART1_CR1      (*(volatile uint32_t*)(USART1_BASE + 0x00))
#define USART1_BRR      (*(volatile uint32_t*)(USART1_BASE + 0x08))
#define USART1_SR       (*(volatile uint32_t*)(USART1_BASE + 0x0C))
#define USART1_DR       (*(volatile uint32_t*)(USART1_BASE + 0x24))

/* Máscaras de bits para periféricos */
#define RCC_IOPENR_GPIOAEN  (1 << 0)
#define RCC_APB2ENR_USART1EN (1 << 14)
#define USART_CR1_UE        (1 << 13)
#define USART_CR1_TE        (1 << 3)
#define USART_SR_TXE        (1 << 7)

#define FNV_32_PRIME        0x01000193

/* Prototipos de funciones */
void Reset_Handler(void);
int main(void);

extern uint32_t _estack;

/* -------------------------------------------------------------------------
   VECTOR DE INTERRUPCIONES MÍNIMO (Corregido a arreglo de punteros void)
   ------------------------------------------------------------------------- */
__attribute__((section(".isr_vector"), used))
void *const g_pfnVectors[] = {
    (void *)&_estack,        /* El puntero de pila inicial */
    (void *)Reset_Handler    /* El vector de reinicio */
};

/* -------------------------------------------------------------------------
   VARIABLES GLOBALES EN SECCIÓN .NOINIT (Preservan el caos cuántico de la RAM)
   ------------------------------------------------------------------------- */
__attribute__((section(".noinit"))) uint32_t xoshiro_s[4];
__attribute__((section(".noinit"))) uint32_t clave_dinamica;

/* Variables de control del Linker para el startup */
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

/* -------------------------------------------------------------------------
   EXTRACCIÓN DE ENTROPÍA EN C PURO (Se ejecuta inmediatamente al resetear)
   ------------------------------------------------------------------------- */
void extraer_entropia_sram(void) {
    // Verificar si el origen de reset es Power-On Reset (Bit 27 de RCC_CSR)
    if (RCC_CSR & (1 << 27)) {
        
        // Limpiar bandera PORRSTF escribiendo RMVF (Bit 24 de RCC_CSR)
        RCC_CSR |= (1 << 24);

        // Constantes iniciales FNV-1a Offset Basis
        uint32_t s0 = 0x811C9DC5;
        uint32_t s1 = 0xA23D4E12;
        uint32_t s2 = 0xC45F6F34;
        uint32_t s3 = 0xE67A8056;

        // Puntero base a la SRAM (0x20000000) interpretado como palabras de 32 bits
        volatile uint32_t *ram_ptr = (volatile uint32_t *)0x20000000;

        // Dividimos los 3 KB de RAM en 4 bloques de 768 bytes (192 palabras de 32 bits cada uno)
        // Cada bloque alimenta un acumulador independiente en paralelo
        for (int i = 0; i < 192; i++) {
            s0 = (s0 ^ ram_ptr[i]) * FNV_32_PRIME;
            s1 = (s1 ^ ram_ptr[i + 192]) * FNV_32_PRIME;
            s2 = (s2 ^ ram_ptr[i + 384]) * FNV_32_PRIME;
            s3 = (s3 ^ ram_ptr[i + 576]) * FNV_32_PRIME;
        }

        // Volcar los 128 bits resultantes al estado de Xoshiro
        xoshiro_s[0] = s0;
        xoshiro_s[1] = s1;
        xoshiro_s[2] = s2;
        xoshiro_s[3] = s3;

        // Licuación cruzada final de los estados para generar la clave secreta dinámica
        clave_dinamica = (s0 ^ s3) + s1;
    }
}

/* -------------------------------------------------------------------------
   MANEJADOR DE RESET PRINCIPAL
   ------------------------------------------------------------------------- */
__attribute__((noreturn))
void Reset_Handler(void) {
    // 1. Extraer el ruido de la RAM antes de inicializar las secciones .data o .bss
    extraer_entropia_sram();

    // 2. Copiar la sección .data desde la Flash a la RAM
    uint32_t *src = &_sidata;
    uint32_t *dest = &_sdata;
    while (dest < &_edata) {
        *dest++ = *src++;
    }

    // 3. Poner a cero la sección .bss
    dest = &_sbss;
    while (dest < &_ebss) {
        *dest++ = 0;
    }

    // 4. Saltar a la aplicación principal
    main();

    while (1);
}

/* -------------------------------------------------------------------------
   ALGORITMO PRNG CON LICUADORA CRIPTOGRÁFICA
   ------------------------------------------------------------------------- */
uint32_t prng_next_cripto(void) {
    /* 1. Avance del estado matemático (Xoshiro128**) */
    const uint32_t rot_result = xoshiro_s[0] * 5;
    uint32_t x = ((rot_result << 7) | (rot_result >> 25)) * 9;

    const uint32_t t = xoshiro_s[1] << 9;
    xoshiro_s[2] ^= xoshiro_s[0];
    xoshiro_s[3] ^= xoshiro_s[1];
    xoshiro_s[1] ^= xoshiro_s[2];
    xoshiro_s[0] ^= xoshiro_s[3];
    xoshiro_s[2] ^= t;
    xoshiro_s[3] = (xoshiro_s[3] << 11) | (xoshiro_s[3] >> 21);

    /* 2. Licuadora no lineal irreversible con inyección de la clave física */
    x ^= clave_dinamica;
    x = ((x << 13) | (x >> 19)) ^ 0x7E3779B9;
    x *= 0x85ebca6b;

    x += clave_dinamica;
    x ^= (x >> 15);
    x *= 0xc2b2ae35;
    x ^= clave_dinamica;

    return x;
}

/* -------------------------------------------------------------------------
   APLICACIÓN PRINCIPAL (Corregida)
   ------------------------------------------------------------------------- */
int main(void) {
    /* 1. Respaldar estado si Xoshiro despertó en ceros (Estado muerto) */
    if (xoshiro_s[0] == 0 && xoshiro_s[1] == 0 && xoshiro_s[2] == 0 && xoshiro_s[3] == 0) {
        xoshiro_s[0] = 0x12345678;
        xoshiro_s[1] = 0x9ABCDEF0;
        xoshiro_s[2] = 0x0FEDCBA9;
        xoshiro_s[3] = 0x87654321;
        clave_dinamica = 0x55AA55AA;
    }

    /* 2. Inicializar relojes de GPIOA y USART1 */
    RCC_IOPENR  |= RCC_IOPENR_GPIOAEN;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN; // <--- CORREGIDO: Activa el reloj de USART1 (Bit 14)

    // Configurar PA2 como Función Alterna para USART1_TX
    GPIOA_MODER &= ~(3 << (2 * 2));
    GPIOA_MODER |=  (2 << (2 * 2));
    GPIOA_AFRL  &= ~(0xF << (2 * 4));
    GPIOA_AFRL  |=  (1 << (2 * 4)); // AF1 para USART1_TX

    /* 3. Configurar el Baud Rate a exactamente 1,000,000 baudios (Divisor = 24) */
    USART1_BRR = 24;

    /* 4. Habilitar Transmisor (TE) y el periférico completo (UE) */
    USART1_CR1 |= USART_CR1_TE | USART_CR1_UE;

    /* 5. Streaming infinito de bytes aleatorios (1 bit aleatorio cosechado por ciclo) */
    uint8_t byte_a_transmitir;
    while (1) {
        byte_a_transmitir = 0;

        byte_a_transmitir |= (prng_next_cripto() & 1) << 0;
        byte_a_transmitir |= (prng_next_cripto() & 1) << 1;
        byte_a_transmitir |= (prng_next_cripto() & 1) << 2;
        byte_a_transmitir |= (prng_next_cripto() & 1) << 3;
        byte_a_transmitir |= (prng_next_cripto() & 1) << 4;
        byte_a_transmitir |= (prng_next_cripto() & 1) << 5;
        byte_a_transmitir |= (prng_next_cripto() & 1) << 6;
        byte_a_transmitir |= (prng_next_cripto() & 1) << 7;

        // Esperar a que el buffer de hardware TXE esté libre
        while (!(USART1_SR & USART_SR_TXE));

        // Transmitir byte por el pin PA2
        USART1_DR = byte_a_transmitir;
    }

    return 0;
}
