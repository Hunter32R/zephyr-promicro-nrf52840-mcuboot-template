/*
   * Retained memory section (retained RAM)
   *
   * Address: 0x2000FFC0 - Specific RAM region of nRF52840
   * Size: 64 bytes (0x40 bytes) - Minimum area for some variables
   *
   */
  .retained_ram 0x2003FFC0 (NOLOAD) : ALIGN(4) {
    __retained_ram_start = .;     /* Marks the beginning of the section */
    KEEP(*(.retained_ram))        /* Keeps all symbols from the .retained_ram section */
    __retained_ram_end = .;       /* Marks the end of the section */
  }
