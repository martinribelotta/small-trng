# =============================================================================
# Makefile para Generador TRNG Puya PY32F002A
# Optimizado para Depuración (-Og) y Eliminación de Secciones Muertas (--gc-sections)
# =============================================================================

# Nombre del proyecto y archivos de salida
TARGET = trng_usart

# Herramientas de compilación de los repositorios
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# Archivos fuente y script de enlazado
SRCS   = main.c
LINKER = main.ld

# Parámetros de arquitectura (ARM Cortex-M0+ puro)
MCU_FLAGS = -mcpu=cortex-m0plus -mthumb

# Opciones de compilación:
# -Og: Optimización dirigida a la depuración (mantiene el código rastreable)
# -Wall: Activar todas las advertencias
# -nostartfiles: No incluir las bibliotecas de arranque estándar de GCC
# -ffunction-sections -fdata-sections: Coloca cada función y variable en su propia sección
CFLAGS = $(MCU_FLAGS) -Og -Wall -nostartfiles -ffunction-sections -fdata-sections

# Opciones del enlazador (Linker):
# -T: Especificar el script linker personalizado
# -Wl,-Map: Generar archivo de mapa de memoria para auditoría
# -Wl,--gc-sections: Solicita al linker eliminar todas las secciones de código/datos no usadas
LDFLAGS = -T $(LINKER) -Wl,-Map=$(TARGET).map -Wl,--gc-sections

# Regla por defecto (Se ejecuta al escribir 'make' en la terminal)
all: $(TARGET).bin size

# Compilación y enlazado directo
$(TARGET).elf: $(SRCS) $(LINKER)
	@echo "Compilando con -Og y preparando secciones..."
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

# Extracción del binario puro para el programador
$(TARGET).bin: $(TARGET).elf
	@echo "Generando imagen binaria cruda (.bin)..."
	$(OBJCOPY) -O binary $< $@

# Mostrar reporte de consumo de memoria Flash y RAM
size: $(TARGET).elf
	@echo "\n--- Reporte de ocupación en el Silicio ---"
	$(SIZE) $<
	@echo "Nota: El recolector de basura (gc-sections) ya eliminó el código muerto."

# Limpieza de archivos generados ('make clean')
clean:
	@echo "Limpiando binarios y archivos temporales..."
	rm -f $(TARGET).elf $(TARGET).bin $(TARGET).map

.PHONY: all size clean
