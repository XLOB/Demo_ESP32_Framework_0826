/**
 * @file app_temp_task.h
 * @brief 温度监测任务
 *
 * 周期性读取内部温度传感器并输出日志。
 */
#ifndef APP_TEMP_TASK_H
#define APP_TEMP_TASK_H

/**
 * @brief 温度监测任务入口
 * @param arg  任务参数（未使用）
 */
void temp_task(void *arg);

#endif /* APP_TEMP_TASK_H */
