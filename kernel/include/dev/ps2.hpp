#pragma once

struct tty_device;

void ps2_init();
void ps2_set_tty(tty_device* tty);
