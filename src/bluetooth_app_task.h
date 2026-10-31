#ifndef EEVBLOG_BLUETOOTH_APP_TASK_H
#define EEVBLOG_BLUETOOTH_APP_TASK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BLUETOOTH_APP_EVENT_NONE = 0,
    BLUETOOTH_APP_EVENT_UART_COMMAND,
    BLUETOOTH_APP_EVENT_BUTTON,
    BLUETOOTH_APP_EVENT_DATETIME,
} bluetooth_app_event_kind_t;

typedef struct {
    bluetooth_app_event_kind_t kind;
    uint8_t value;
    uint8_t datetime[6];
} bluetooth_app_event_t;

void bluetooth_app_task_init(void);
bool bluetooth_app_task_poll(bool stock_app_mode, bluetooth_app_event_t *event);
void bluetooth_app_task_service(void);
bool bluetooth_app_task_take_event(bluetooth_app_event_t *event);

#endif
