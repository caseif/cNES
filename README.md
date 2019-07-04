# cNES

cNES is a cycle-accurate NES emulator written in C.

cNES is partially derived from the now-defunct
[jNES](https://github.com/caseif/jNES).

### Features

- Support for popular mappers (NROM, UNROM, MMC1, MMC3)
- Cycle-accurate CPU and PPU emulation
- Low-level emulation of PPU hardware latches/registers

### Limitations

- No APU support
- Attributes are a bit buggy on the left edge of the screen
- PPU memory access is not cycle-accurate (it's done as a single operation at
  the moment)
- Interrupt timing is imperfect
- No support for color masking

### Planned Features

- APU support
- More mapper implementations
- Cycle accurate memory access for PPU fetching
- Color masking support

### Non-goals

- User-friendliness
    - This project is a technical challenge for myself, and as such a 
      user-friendly interface is not one of its goals. This is not to say that
      it will never exist, just that it's not a priority.

### Why?

Mostly for educational purposes, and also for fun. Don't expect too much out of
it.

### License

cNES is published under the [MIT License](https://opensource.org/licenses/MIT).
Use of its code and any provided assets is permitted per the terms of the
license.
