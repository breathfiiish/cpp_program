/* * Copyright ? 2010-2020 BYD Corporation. All rights reserved.
*
* BYD Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from BYD Corporation is strictly prohibited.
*
*/
#ifndef AUTO_LOGS_H
#define AUTO_LOGS_H

//#include "utils/Log.h"
#include "android/logger_write.h"
#include "ConfigList.h"

namespace byd_auto_hal {

static int loglevel = 2;
static bool logprint = true;

#ifndef ALOGD0
#define ALOGD0(...) AUTO_LOGD_IF((loglevel >= 0), __VA_ARGS__);
#endif

#ifndef ALOGD1
#define ALOGD1(...) AUTO_LOGD_IF((logprint && loglevel >= 1), __VA_ARGS__);
#endif

#ifndef ALOGD2
#define ALOGD2(...) AUTO_LOGD_IF((logprint && loglevel >= 2), __VA_ARGS__);
#endif

#ifndef ALOGD3
#define ALOGD3(...) AUTO_LOGD_IF((logprint && loglevel >= 3), __VA_ARGS__);
#endif

#ifndef START_LOG
#define START_LOG()         \
    do {                    \
        logprint = true;    \
    } while(0)
#endif

#ifndef STOP_LOG
#define STOP_LOG()          \
    do {                    \
        logprint = false;   \
    } while(0)
#endif

#ifndef SET_LOG_LEVEL
#define SET_LOG_LEVEL(l)    \
    do {                    \
        loglevel = l;       \
    } while(0)
#endif

#ifndef PRINT_BUF_LOG
#define PRINT_BUF_LOG(buf, len)                                                         \
    do {                                                                                \
        if (logprint && (loglevel >= 2)) {                                              \
            char _d[0x36 * 3 + 20];                                                     \
            char* _p = _d;                                                              \
            int _i = 0, min = (len <= 0x36) ? len : 0x36;                               \
            char _h = 0, _l = 0;                                                        \
            for (_i = 0; _i < min; _i++) {                                              \
                _h = ((buf[_i] >> 4) & 0xF);                                            \
                _l = (buf[_i] & 0xF);                                                   \
                _h += (_h >= 0 && _h <= 9) ? 0x30 : 0x37;                               \
                _l += (_l >= 0 && _l <= 9) ? 0x30 : 0x37;                               \
                *_p++ = _h;                                                             \
                *_p++ = _l;                                                             \
                *_p++ = ' ';                                                            \
            }                                                                           \
            _d[min * 3] = '\0';                                                         \
            ALOGD2("%s[%d/%d]", _d, _i, len);                                           \
        }                                                                               \
    } while(0)
#endif

#ifndef PRINT_BUF_LOG_OB
#define PRINT_BUF_LOG_OB(buf, len,s_count,d_count,vcuCount,len_count)                                      \
    do {                                                                                \
        if (logprint && (loglevel >= 2)) {                                              \
            char _d[len * 3 + 20];                                                      \
            char* _p = _d;                                                              \
            int _i = 0, min = len;                                                      \
            char _h = 0, _l = 0;                                                        \
            for (_i = 0; _i < min; _i++) {                                              \
                _h = ((buf[_i] >> 4) & 0xF);                                            \
                _l = (buf[_i] & 0xF);                                                   \
                _h += (_h >= 0 && _h <= 9) ? 0x30 : 0x37;                               \
                _l += (_l >= 0 && _l <= 9) ? 0x30 : 0x37;                               \
                *_p++ = _h;                                                             \
                *_p++ = _l;                                                             \
                *_p++ = ' ';                                                            \
            }                                                                           \
            _d[min * 3] = '\0';                                                         \
            ALOGD2("%s[%d/%d][sc:%u] [dc:%u] [vc:%u] [len:%u]", _d, _i, len,s_count,d_count,vcuCount,len_count);            \
        }                                                                               \
    } while(0)
#endif

#ifndef PRINT_DYNAMIC_DATA
#define PRINT_DYNAMIC_DATA(buf, len, s_count,d_count,vcuCount,len_count)                                   \
    do {                                                                                \
        if (logprint && (loglevel >= 3)) {                                              \
            char _d[len * 3 + 20];                                                      \
            char* _p = _d;                                                              \
            int _i = 0, min = len;                                                      \
            char _h = 0, _l = 0;                                                        \
            for (_i = 0; _i < min; _i++) {                                              \
                _h = ((buf[_i] >> 4) & 0xF);                                            \
                _l = (buf[_i] & 0xF);                                                   \
                _h += (_h >= 0 && _h <= 9) ? 0x30 : 0x37;                               \
                _l += (_l >= 0 && _l <= 9) ? 0x30 : 0x37;                               \
                *_p++ = _h;                                                             \
                *_p++ = _l;                                                             \
                *_p++ = ' ';                                                            \
            }                                                                           \
            _d[min * 3] = '\0';                                                         \
            ALOGD3("%s[%d/%d] [sc:%u] [dc:%u] [vc:%u] [len:%u]", _d, _i, len,s_count,d_count,vcuCount,len_count);           \
        }                                                                               \
    } while(0)
#endif

};
#endif