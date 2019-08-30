# cNES

cNES is a cycle-accurate NES emulator written in C.

cNES is partially derived from the now-defunct
[jNES](https://github.com/caseif/jNES).

### Features

- Support for popular mappers (NROM, MMC1, UNROM, CNROM, MMC3)
- Cycle-accurate CPU and PPU emulation
- Low-level emulation of PPU hardware latches/registers

### Limitations

- No APU support
- PPU timings are juuust a little bit off
- No support for color masking
- Many games are broken in one way or another (see [compatibility list](doc/COMPATIBILITY.md))

### Planned Features

- APU support
- More mapper implementations
- Color masking support

### Non-goals

- User-friendliness
    - This project is a technical challenge for myself, and as such a 
      user-friendly interface is not one of its goals. This is not to say that
      it will never exist, just that it's not a priority.
- Save states
    - Again, I'm interested in adding such a feature, but it's definitely not a
      priority.

### Why?

Mostly for educational purposes, and also for fun.

### Special Thanks To

- The NesDev [forums](https://forums.nesdev.com/) and
  [wiki](http://wiki.nesdev.com), which have been an invaluable resource in
  building this project
- [blargg](http://blargg.8bitalley.com/), whose tests have helped immensely with
  getting details (both large and small) locked down
- Everyone responsible for the documents listed on the
  [resources page](doc/RESOURCES.md)

### License

cNES is published under the [MIT License](https://opensource.org/licenses/MIT).
Use of its code and any provided assets is permitted per the terms of the
license.
