#pragma once

void klog_init();
void klog_write_nolock(const char* message);
void klog_nolock(const char* fmt_string, ...);
void klog(const char* fmt_string, ...);
