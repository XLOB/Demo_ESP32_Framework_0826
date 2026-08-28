#ifndef APP_TEMP_H
#define APP_TEMP_H

struct Device; // 前置声明

struct Device *InternalTemp_get_device(void);

void temp_task(void *arg);

#endif