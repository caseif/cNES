# cNES

cNES is a NES emulator written in C.

cNES is partially derived from the now-defunct [jNES](https://github.com/caseif/jNES).

### Features

- NROM/MMC3 mapper support

##### CPU

- Instruction-granular emulation

##### PPU

- Cycle-granular emulation
- Low-level emulation of hardware latches/registers

### Limitations

- Tile attributes are buggy with fine x-scroll
- CPU is not cycle-accurate
  - Interrupts may be slightly mis-timed depending on their alignment with instruction execution
- PPU memory access is not cycle-accurate (it's done as a single operation at the moment)
- MMC3 implementation is a bit buggy
- No support for color masking

### Planned Features

- More mapper implementations
- Cycle-accurate CPU emulation
- Cycle accurate memory access for PPU fetching
- Color masking support

### Why?

Mostly for educational purposes, and also for fun. Don't expect too much out of it.

### License

cNES is published under the [MIT License](https://opensource.org/licenses/MIT).
Use of its code and any provided assets is permitted per the terms of the license.
