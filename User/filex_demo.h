#ifndef FILEX_DEMO_H
#define FILEX_DEMO_H

/*
 * FileX demo: format the W25Q128 SPI flash (if needed), create a
 * file, write data, read it back and verify, then list the directory.
 * Runs in its own low-priority ThreadX thread.
 */

void filex_demo_start(void);

#endif
