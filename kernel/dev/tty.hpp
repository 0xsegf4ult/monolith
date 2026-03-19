#pragma once

struct tty_device;

void tty_init();
void tty_consume(tty_device* tty, char c);
