/**
 * @file app_key_task.h
 * @brief 按键任务
 *
 * 轮询按键状态并发送按键事件到全局消息队列。
 */
#ifndef APP_KEY_TASK_H
#define APP_KEY_TASK_H

/**
 * @brief 按键任务入口
 *
 * 周期性轮询所有按键设备，检测到按下时
 * 将按键 ID 发送到 key_queue 消息队列。
 *
 * @param arg  任务参数（未使用）
 */
void key_task(void *arg);

#endif /* APP_KEY_TASK_H */
