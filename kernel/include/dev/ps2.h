#pragma once

struct tty_device;

void ps2_init();
void ps2_set_tty(struct tty_device* tty);
