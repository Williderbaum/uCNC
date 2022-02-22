/*
    Name: module.h
    Description: Module extensions for µCNC.
    All entry points for extending µCNC core functionalities are declared in this module

    Copyright: Copyright (c) João Martins
    Author: João Martins
    Date: 21-02-2022

    µCNC is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version. Please see <http://www.gnu.org/licenses/>

    µCNC is distributed WITHOUT ANY WARRANTY;
    Also without the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the	GNU General Public License for more details.
*/

#ifndef MODULE_H
#define MODULE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../cnc_config.h"
#include "core/parser.h"

// definitions to create events and event listeners
#define EVENT(delegate)                 \
    typedef struct delegate##_event_    \
    {                                   \
        delegate fptr;                  \
        struct delegate##_event_ *next; \
    } delegate##_event_t;               \
    delegate##_event_t *
#define EVENT_TYPE(delegate) delegate##_event_t
#define CREATE_LISTENER(delegate, handler) __attribute__((used)) delegate##_event_t delegate##_##handler = {&handler, NULL}
#define ADD_LISTENER(delegate, handler, event) ({   \
    extern delegate##_event_t delegate##_##handler; \
    delegate##_event_t **p = &event;                \
    while ((*p) != NULL)                            \
    {                                               \
        (*p) = (*p)->next;                          \
    }                                               \
    (*p) = &delegate##_##handler;                   \
    (*p)->next = NULL;                              \
})

// definitions to create overridable default handlers for functions void-void hooks
// the resulting handles is named mod_<hookname>_hook and can be placed inside any point in the core code
#define DECL_DEFAULT_HANDLER(hook)         \
    typedef void (*hook##_delegate)(void); \
    EVENT(hook##_delegate)                 \
    hook##_event;                          \
    void __attribute__((weak)) mod_##hook##_hook(void)
#define DEFAULT_HANDLER(hook) ({                     \
    EVENT_TYPE(hook##_delegate) *ptr = hook##_event; \
    while (ptr != NULL)                              \
    {                                                \
        if (ptr->fptr != NULL)                       \
        {                                            \
            ptr->fptr();                             \
        }                                            \
        ptr = ptr->next;                             \
    }                                                \
})

#ifdef ENABLE_PARSER_MODULES
    // defines a delegate function for the gcode parser handler
    typedef uint8_t (*gcode_parse_delegate)(unsigned char, uint8_t, uint8_t, float, parser_state_t *, parser_words_t *, parser_cmd_explicit_t *);
    // creates an event for the gcode_parse
    EVENT(gcode_parse_delegate)
    gcode_parse_event;
    // declares the handler hook to be called inside the parser core
    uint8_t mod_gcode_parse_hook(unsigned char word, uint8_t code, uint8_t error, float value, parser_state_t *new_state, parser_words_t *words, parser_cmd_explicit_t *cmd);

    // defines a delegate function for the gcode exec handler
    typedef uint8_t (*gcode_exec_delegate)(parser_state_t *, parser_words_t *, parser_cmd_explicit_t *);
    // creates an event for the gcode_exec
    EVENT(gcode_exec_delegate)
    gcode_exec_event;
    // declares the handler hook to be called inside the parser core
    uint8_t mod_gcode_exec_hook(parser_state_t *new_state, parser_words_t *words, parser_cmd_explicit_t *cmd);
#endif

#ifdef ENABLE_SCHEDULER_LOOP_MODULES
    // generates a default delegate, event and handler for rtc_tick
    // the only thing left is to include de call to the handler (mod_rtc_tick_hook) inside the cnc core
    DECL_DEFAULT_HANDLER(rtc_tick)
    {
        DEFAULT_HANDLER(rtc_tick);
    }
#endif

#ifdef ENABLE_IO_MODULES
    DECL_DEFAULT_HANDLER(input_change)
    {
        DEFAULT_HANDLER(input_change);
    }
    DECL_DEFAULT_HANDLER(probe_enable)
    {
    }
    DECL_DEFAULT_HANDLER(probe_disable)
    {
    }
#endif

    void mod_init(void);

#ifdef __cplusplus
}
#endif

#endif
