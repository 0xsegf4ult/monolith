#pragma once

void panic_prepare();
void panic_complete();
void panic(const char* fmt, ...);
