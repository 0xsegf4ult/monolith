#pragma once

struct tty_device;

namespace ps2
{

void init();
void set_tty(tty_device* tty);

}
