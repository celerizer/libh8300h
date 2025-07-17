# libh8300h

![Stepdad_k3bHvQzzLc](https://github.com/user-attachments/assets/b9f266ea-cb6c-4f5d-bbb4-b6a1f20bdfa8)

**libh8300h** (name pending) is an emulation library for the Hitachi H8/300H, developed for the purpose of writing Stepdad, an emulator for small pedometer devices.

libh8300h is written to maintain maximum compatibility across different build targets by adhering to C89 standards, minimizing the use of standard libraries, and providing extensive configuration options.

## Building

- Include `libh8300h.mk` in your project makefile:

```make
include libh8300h/libh8300h.mk
```

- Add the exported `H8_SOURCES` and `H8_HEADERS` to your project makefile:

```make
SOURCES += $(H8_SOURCES)
HEADERS += $(H8_HEADERS)
```

- Add any compile-time options you wish to change. A list of all compile-time options along with their default values can be found in `config.h`. As an example:

```make
CFLAGS += \
  -DH8_BIG_ENDIAN=1 \
  -DH8_REVERSE_BITFIELDS=1
```

This will build for a big-endian target with bitfields represented as MSB-first.

## Usage

- The following example code can be used to initialize an NTR-032 console:

```c
h8_system_t system;

/* Initialize an H8/300H system */
h8_init(&system);

/* Hook up all devices of a specified console */
h8_system_init(&system, H8_SYSTEM_NTR_032);

/* Add some program data */
h8_write(&system, /* pointer to program data */, 0, /* size of program data */, 0);
```

- Then, the system can begin processing using:

```c
/* Process one instruction... */
h8_step(&system);
```

## License
**libh8300h** is distributed under the MIT license. See LICENSE for information.

