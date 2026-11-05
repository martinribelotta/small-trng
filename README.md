# Bare-Metal SRAM-Based TRNG for Puya PY32F002A

A low-cost, high-performance True Random Number Generator (TRNG) architecture implemented on an ultra-cheap Puya PY32F002A (ARM Cortex-M0+, SOP8 package) running at 24 MHz without using ADCs, hardware timers, or external oscillators.

This project extracts physical entropy from the chaotic uninitialized startup states of the internal SRAM (quantum/thermal noise) to feed a cryptographically shielded **Xoshiro128**** Pseudo-Random Number Generator (PRNG). The system outputs a clean, infinite stream of random data over an asynchronous **USART link at 1,000,000 baud (1 Mbps)**.

## 🚀 Key Features

*   **Zero External Components:** Operates autonomously using only the MCU and a single 100nF decoupling capacitor.
*   **Pure C Implementation:** No fragile or compiler-dependent inline assembly blocks; utilizes architecture-safe C routines optimized by GCC for Thumb-16.
*   **True Entropy Bootstrapping:** Loops through 3 KB of raw SRAM in parallel before the C-runtime (`crt0`) initializes variables, securing **~240 bits of pure quantum/thermal noise**.
*   **Cryptographic Shielding:** Implements an irreversible non-linear Substitution-Permutation Network (SPN) "scrambler" mixed with a dynamic secret key generated during cold-boot, preventing attackers from back-tracking the generator's state via linear cryptoanalysis.
*   **Optimized Bare-Metal Build:** Custom linker script preserves uninitialized sections (`.noinit`) across resets and enables dead-code elimination (`--gc-sections`).

## 🔬 Theory of Operation

### 1. Entropy Harvesting (True RNG Phase)
When an SRAM cell (formed by cross-coupled inverters) is powered on from a pure 0V state, it enters a quantum-level race condition. Microscopic manufacturing variations and instantaneous thermal noise force a fraction of these cells (~1% to 5%) to wake up in an unpredictable state (`0` or `1`).

The firmware splits the 3 KB SRAM into 4 independent blocks of 768 bytes. During a **Power-On Reset (POR)**, a parallel FNV-1a hash algorithm compresses the raw entropy of these blocks into four 32-bit accumulators, yielding a **128-bit seeding vector** and a **32-bit dynamic secret key**.

### 2. High-Speed Expansion (PRNG Phase)
Once initialized, the system switches to the mathematical **Xoshiro128**** generator. This algorithm provides an immense period of **2¹²⁸ - 1**, ensuring that even when streaming data at 1 Mbps, the sequence will take longer than the age of the universe to repeat its cycle.

### 3. The Cleft-Bit Scrambler (Anti-Inference Layer)
Standard Xoshiro outputs can be reversed to discover the internal state if an observer captures 4 consecutive 32-bit values. To block this, every bit harvested passes through a multi-stage non-linear blender mixed with the unique `clave_dinamica` (dynamic key) held in memory. Because the key changes physically on every cold boot and is never stored in flash, the stream becomes mathematically irreversible.

## 📦 Project Structure

*   `main.c`: Contains the vector table, raw memory boot-routine, crypto-scrambler, peripheral clock setup, and the infinite bit-harvesting transmit loop.
*   `main.ld`: Custom linker script mapping the 20KB Flash, 3KB RAM, stack pointers, and the vital `.noinit` safety boundary.
*   `Makefile`: Automates cross-compilation with optimization flags tuned for debugging and code size tracking (`-Og`, `-ffunction-sections`, `--gc-sections`).

## 🛠️ Compilation & Deployment

### Prerequisites
Ensure you have the GNU Arm Embedded Toolchain installed on your host system:

```bash
# Ubuntu / Debian
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi make
```

### Building the Project
Clone this repository, navigate to the folder, and run:

```bash
make
```

The build system will output a raw binary file named `trng_usart.bin` and print out a detailed memory footprint report:

```text
--- Reporte de ocupación en el Silicio ---
   text    data     bss     dec     hex filename
    840       0      16     856     358 trng_usart.elf
```

### Flashing
Flash the compiled `trng_usart.bin` to your Puya PY32F002A at address `0x08000000` using an ST-Link, J-Link, or an open-source tool like `pyocd`.

## 🔌 Hardware Connection (SOP8)

Connect your target device to an industrial-grade USB-to-TTL serial adapter (e.g., **FTDI FT232RL** or **CP2102N** with 0% baud rate error at high speeds):

```text
                   Puya PY32F002A (SOP8)
                 ┌───────┐
                 │1     8├── 
  (TX Out) PA2 ──┤2     7├── 
         [VSS] ──┤3     6├── [VDD] ◄─────── VCC (3.3V - 5V)
                 │4     5├── 
                 └───────┘
```

*   **Pin 2 (PA2 / USART1_TX):** Connect to the **RX** pin of your serial adapter.
*   **Pin 3 (VSS):** Connect to **GND** of your serial adapter and power supply.
*   **Pin 6 (VDD):** Connect to **3.3V** or **5.0V**.

⚠️ **Important:** To ensure high-quality entropy, the power supply must decay completely to **0V** during power-off. Rapid soft-resets ("Warm Boots") will allow the SRAM to retain electrical configurations from its previous session, degrading the initial seed's randomness.

## 📈 Verifying Randomness

Configure your host reading terminal or script (e.g., Python using `pyserial`) to:

*   **Baud rate:** `1000000` (1 Mbps)
*   **Data bits:** `8`
*   **Parity:** `None`
*   **Stop bits:** `1`

The serial stream provides a continuous throughput of **800 kbps of raw entropy** (accounting for standard UART 8N1 framing overhead). The resulting binary file can be fed directly into standard statistical suites like *Dieharder* or *NIST SP 800-22* for validation.
