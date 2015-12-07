/*
 * audio-hal
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ALSA_UCM_DEBUG_TIME
#include <sys/time.h>
#include <time.h>
#endif

#include "tizen-audio-internal.h"

#ifdef ALSA_UCM_DEBUG_TIME
#define SND_USE_CASE_SET __set_use_case_with_time
#else
#define SND_USE_CASE_SET snd_use_case_set
#endif

audio_return_t _audio_ucm_init(audio_hal_t *ah)
{
    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);

    snd_use_case_mgr_open(&ah->ucm.uc_mgr, ALSA_DEFAULT_CARD);

    if (!ah->ucm.uc_mgr) {
        AUDIO_LOG_ERROR("uc_mgr open failed");
        return AUDIO_ERR_RESOURCE;
    }
    return AUDIO_RET_OK;
}

audio_return_t _audio_ucm_deinit(audio_hal_t *ah)
{
    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(ah->ucm.uc_mgr, AUDIO_ERR_PARAMETER);

    snd_use_case_mgr_close(ah->ucm.uc_mgr);
    ah->ucm.uc_mgr = NULL;

    return AUDIO_RET_OK;
}

void _audio_ucm_get_device_name(audio_hal_t *ah, const char *use_case, audio_direction_t direction, const char **value)
{
    char identifier[70] = { 0, };

    AUDIO_RETURN_IF_FAIL(ah);
    AUDIO_RETURN_IF_FAIL(ah->ucm.uc_mgr);

    snprintf(identifier, sizeof(identifier), "%sPCM//%s",
             (direction == AUDIO_DIRECTION_IN)? "Capture" : "Playback", use_case);

    snd_use_case_get(ah->ucm.uc_mgr, identifier, value);
}

static inline void __add_ucm_device_info(audio_hal_t *ah, const char *use_case, audio_direction_t direction, audio_device_info_t *device_info_list, int *device_info_count)
{
    audio_device_info_t *device_info;
    const char *device_name = NULL;
    char *needle = NULL;

    AUDIO_RETURN_IF_FAIL(ah);
    AUDIO_RETURN_IF_FAIL(ah->ucm.uc_mgr);
    AUDIO_RETURN_IF_FAIL(device_info_list);
    AUDIO_RETURN_IF_FAIL(device_info_count);

    _audio_ucm_get_device_name(ah, use_case, direction, &device_name);
    if (device_name) {
        device_info = &device_info_list[(*device_info_count)++];

        memset(device_info, 0x00, sizeof(audio_device_info_t));
        device_info->api = AUDIO_DEVICE_API_ALSA;
        device_info->direction = direction;
        needle = strstr(&device_name[3], ",");
        if (needle) {
            device_info->alsa.device_idx = *(needle+1) - '0';
            device_info->alsa.card_name = strndup(&device_name[3], needle - (device_name+3));
            device_info->alsa.card_idx = snd_card_get_index(device_info->alsa.card_name);
            AUDIO_LOG_DEBUG("Card name: %s", device_info->alsa.card_name);
        }

        free((void *)device_name);
    }
}

int _audio_ucm_fill_device_info_list(audio_hal_t *ah, audio_device_info_t *device_info_list, const char *verb)
{
    int device_info_count = 0;
    const char *curr_verb = NULL;

    AUDIO_RETURN_VAL_IF_FAIL(ah, device_info_count);
    AUDIO_RETURN_VAL_IF_FAIL(ah->ucm.uc_mgr, device_info_count);
    AUDIO_RETURN_VAL_IF_FAIL(device_info_list, device_info_count);

    if (!verb) {
        snd_use_case_get(ah->ucm.uc_mgr, "_verb", &curr_verb);
        verb = curr_verb;
    }

    /* prepare destination */
    /*If the devices are VOICECALL LOOPBACK or FMRADIO then pulseaudio need not get the device notification*/
    if (verb) {
        if (strncmp(verb, AUDIO_USE_CASE_VERB_VOICECALL, strlen(AUDIO_USE_CASE_VERB_VOICECALL)) &&
            strncmp(verb, AUDIO_USE_CASE_VERB_LOOPBACK, strlen(AUDIO_USE_CASE_VERB_LOOPBACK))) {
            __add_ucm_device_info(ah, verb, AUDIO_DIRECTION_IN, device_info_list, &device_info_count);
            if(strncmp(verb, AUDIO_USE_CASE_VERB_FMRADIO, strlen(AUDIO_USE_CASE_VERB_FMRADIO))) {
                __add_ucm_device_info(ah, verb, AUDIO_DIRECTION_OUT, device_info_list, &device_info_count);
            }
        }

        if (curr_verb)
            free((void *)curr_verb);

    }

    return device_info_count;
}

static void __dump_use_case(const char *verb, const char *devices[], int dev_count, const char *modifiers[], int mod_count, char *dump)
{
    int i, len;

    len = sprintf(dump, "Verb [ %s ] Devices [ ", verb ? verb : AUDIO_USE_CASE_VERB_INACTIVE);
    if (len > 0)
        dump += len;

    if (devices) {
        for (i = 0; i < dev_count; i++) {
            len = sprintf(dump, (i != dev_count - 1)? "%s, " : "%s", devices[i]);
            if (len > 0)
                dump += len;
        }
    }

    len = sprintf(dump, " ] Modifier [ ");
    if (len > 0)
        dump += len;

    if (modifiers) {
        for (i = 0; i < mod_count; i++) {
            len = sprintf(dump, (i != mod_count - 1)? "%s, " : "%s", modifiers[i]);
            if (len > 0)
                dump += len;
        }
    }

    len = sprintf(dump, " ]");
    if (len > 0)
        dump += len;

    *dump = '\0';
}

#ifdef ALSA_UCM_DEBUG_TIME
static inline int __set_use_case_with_time(snd_use_case_mgr_t *uc_mgr, const char *identifier, const char *value)
{
    int ret = 0;
    struct timeval t_start, t_stop;
    unsigned long long t_diff = 0;

    gettimeofday(&t_start, NULL);
    ret = snd_use_case_set(uc_mgr, identifier, value);
    gettimeofday(&t_stop, NULL);
    if (t_start.tv_sec < t_stop.tv_sec)
        t_diff = (t_stop.tv_sec - t_start.tv_sec) * 1000000;
    t_diff += (t_stop.tv_usec - t_start.tv_usec);
    AUDIO_LOG_DEBUG("identifier %s value %s takes %lluusec", identifier, value, t_diff);

    return ret;
}
#endif

/* UCM sequence
    1) If verb is null or verb is not changed
    1-1) If device is changed
         (If there is request for same device, it will be ignored)
         -> Set "Inactive" verb, disable modifiers & devices, set current verb again, enable devices & modifiers
            (playback/capture device will be enabled again if there is no request for playback/capture device)
    1-2) If device is not changed
     1-2-1) If modifier is changed
            (If there is request for same modifier, it will be ignored)
            -> Disable modifiers, enable modifiers
   2) If verb is changed
      -> Reset, set new verb, enable devices & modifiers
 */
audio_return_t _audio_ucm_set_use_case(audio_hal_t *ah, const char *verb, const char *devices[], const char *modifiers[])
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    int is_verb_changed = 0, is_dev_changed = 0, is_mod_changed = 0;
    const char *old_verb = NULL, **old_dev_list = NULL, **old_mod_list = NULL;
    int old_dev_count = 0, dev_count = 0;
    int old_mod_count = 0, mod_count = 0;
    const char **dis_dev_list = NULL, **ena_dev_list = NULL;
    const char **dis_mod_list = NULL, **ena_mod_list = NULL;
    int dis_dev_count = 0, ena_dev_count = 0;
    int dis_mod_count = 0, ena_mod_count = 0;
    int i = 0, j = 0;
    char dump_str[512];

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(ah->ucm.uc_mgr, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(verb, AUDIO_ERR_PARAMETER);

    snd_use_case_get(ah->ucm.uc_mgr, "_verb", &old_verb);
    old_dev_count = snd_use_case_get_list(ah->ucm.uc_mgr, "_enadevs", &old_dev_list);
    old_mod_count = snd_use_case_get_list(ah->ucm.uc_mgr, "_enamods", &old_mod_list);
    __dump_use_case(old_verb, old_dev_list, old_dev_count, old_mod_list, old_mod_count, &dump_str[0]);
    AUDIO_LOG_INFO(">>> UCM current %s", dump_str);

    if (devices) {
        for (dev_count = 0; devices[dev_count]; dev_count++);
    }
    if (modifiers) {
        for (mod_count = 0; modifiers[mod_count]; mod_count++);
    }

    __dump_use_case(verb, devices, dev_count, modifiers, mod_count, &dump_str[0]);
    AUDIO_LOG_INFO("> UCM requested %s", dump_str);

    if (old_verb && streq(verb, old_verb)) {
        AUDIO_LOG_DEBUG("current verb and new verb is same. No need to change verb, disable devices explicitely");

        if (old_dev_count > 0) {
            dis_dev_list = (const char **)malloc(sizeof(const char *) * old_dev_count);
            for (i = 0; i < old_dev_count; i++) {
                dis_dev_list[i] = NULL;
            }
        }
        if (dev_count > 0) {
            ena_dev_list = (const char **)malloc(sizeof(const char *) * dev_count);
            for (i = 0; i < dev_count; i++) {
                ena_dev_list[i] = NULL;
            }
        }
        if (old_mod_count > 0) {
            dis_mod_list = (const char **)malloc(sizeof(const char *) * old_mod_count);
            for (i = 0; i < old_mod_count; i++) {
                dis_mod_list[i] = NULL;
            }
        }
        if (mod_count > 0) {
            ena_mod_list = (const char **)malloc(sizeof(const char *) * mod_count);
            for (i = 0; i < mod_count; i++) {
                ena_mod_list[i] = NULL;
            }
        }

        /* update disable modifiers list which are not present in new modifier list */
        for (i = 0; i < old_mod_count; i++) {
            int need_disable_mod = 1;

            for (j = 0; j < mod_count; j++) {
                if (streq(old_mod_list[i], modifiers[j])) {
                    need_disable_mod = 0;
                    break;
                }
            }
            if (need_disable_mod) {
                if (is_mod_changed == 0)
                    is_mod_changed = 1;
                dis_mod_list[dis_mod_count++] = old_mod_list[i];
            }
        }

        /* update disable devices list which are not present in new device list */
        for (i = 0; i < old_dev_count; i++) {
            int need_disable_dev = 1;

            for (j = 0; j < dev_count; j++) {
                if (streq(old_dev_list[i], devices[j])) {
                    need_disable_dev = 0;
                    break;
                }
            }
            if (need_disable_dev) {
                if (is_dev_changed == 0)
                    is_dev_changed = 1;
                dis_dev_list[dis_dev_count++] = old_dev_list[i];
            }
        }

        /* update enable devices list which are not present in old device list */
        for (i = 0; i < dev_count; i++) {
            int need_enable_dev = 1;

            for (j = 0; j < old_dev_count; j++) {
                if (streq(devices[i], old_dev_list[j])) {
                    need_enable_dev = 0;
                    break;
                }
            }
            if (need_enable_dev) {
                if (is_dev_changed == 0)
                    is_dev_changed = 1;
                ena_dev_list[ena_dev_count++] = devices[i];
            }
        }

        /* update enable modifiers list which are not present in old modifier list */
        for (i = 0; i < mod_count; i++) {
            int need_enable_mod = 1;

            for (j = 0; j < old_mod_count; j++) {
                if (streq(modifiers[i], old_mod_list[j])) {
                    need_enable_mod = 0;
                    break;
                }
            }
            if (need_enable_mod) {
                if (is_mod_changed == 0)
                    is_mod_changed = 1;
                ena_mod_list[ena_mod_count++] = modifiers[i];
            }
        }

        /* disable modifiers */
        for (i = 0; i < dis_mod_count; i++) {
            AUDIO_LOG_INFO("Disable modifier : %s", dis_mod_list[i]);
            if (snd_use_case_set(ah->ucm.uc_mgr, "_dismod", dis_mod_list[i]) < 0)
                AUDIO_LOG_ERROR("disable %s modifier failed", dis_mod_list[i]);
        }

        /* disable devices */
        for (i = 0; i < dis_dev_count; i++) {
            AUDIO_LOG_INFO("Disable device : %s", dis_dev_list[i]);
            if (snd_use_case_set(ah->ucm.uc_mgr, "_disdev", dis_dev_list[i]) < 0)
                AUDIO_LOG_ERROR("disable %s device failed", dis_dev_list[i]);
        }

        /* enable devices */
        for (i = 0; i < ena_dev_count; i++) {
            AUDIO_LOG_INFO("Enable device : %s", ena_dev_list[i]);
            if (snd_use_case_set(ah->ucm.uc_mgr, "_enadev", ena_dev_list[i]) < 0)
                AUDIO_LOG_ERROR("enable %s device failed", ena_dev_list[i]);
        }

        /* enable modifiers */
        for (i = 0; i < ena_mod_count; i++) {
            AUDIO_LOG_INFO("Enable modifier : %s", ena_mod_list[i]);
            if (snd_use_case_set(ah->ucm.uc_mgr, "_enamod", ena_mod_list[i]) < 0)
                AUDIO_LOG_ERROR("enable %s modifier failed", ena_mod_list[i]);
        }
    } else {
        is_verb_changed = 1;

        AUDIO_LOG_DEBUG("Setting new verb: %s", verb);
        /* set new verb */
        if (snd_use_case_set(ah->ucm.uc_mgr, "_verb", verb) < 0) {
            AUDIO_LOG_ERROR("Setting verb %s failed", verb);
            audio_ret = AUDIO_ERR_UNDEFINED;
            goto exit;
        }
        /* enable devices */
        for (i = 0; i < dev_count; i++) {
            AUDIO_LOG_DEBUG("Enable device : %s", devices[i]);
            if(snd_use_case_set(ah->ucm.uc_mgr, "_enadev", devices[i]) < 0)
                AUDIO_LOG_ERROR("Enable %s device failed", devices[i]);
        }
        /* enable modifiers */
        for (i = 0; i < mod_count; i++) {
            AUDIO_LOG_DEBUG("Enable modifier : %s", modifiers[i]);
            if(snd_use_case_set(ah->ucm.uc_mgr, "_enamod", modifiers[i]) < 0)
                AUDIO_LOG_ERROR("Enable %s modifier failed", modifiers[i]);
        }
    }

exit:
    if (old_verb)
        free((void *)old_verb);
    if (old_dev_list)
        snd_use_case_free_list(old_dev_list, old_dev_count);
    if (old_mod_list)
        snd_use_case_free_list(old_mod_list, old_mod_count);
    if (dis_dev_list)
        free((void *)dis_dev_list);
    if (ena_dev_list)
        free((void *)ena_dev_list);
    if (dis_mod_list)
        free((void *)dis_mod_list);
    if (ena_mod_list)
        free((void *)ena_mod_list);

    if (is_verb_changed == 1 || is_dev_changed == 1 || is_mod_changed == 1) {
        const char *new_verb = NULL, **new_dev_list = NULL, **new_mod_list = NULL;
        int new_dev_count = 0, new_mod_count = 0;

        snd_use_case_get(ah->ucm.uc_mgr, "_verb", &new_verb);
        new_dev_count = snd_use_case_get_list(ah->ucm.uc_mgr, "_enadevs", &new_dev_list);
        new_mod_count = snd_use_case_get_list(ah->ucm.uc_mgr, "_enamods", &new_mod_list);
        __dump_use_case(new_verb, new_dev_list, new_dev_count, new_mod_list, new_mod_count, &dump_str[0]);
        AUDIO_LOG_INFO("<<< UCM changed %s", dump_str);

        if (new_verb)
            free((void *)new_verb);
        if (new_dev_list)
            snd_use_case_free_list(new_dev_list, new_dev_count);
        if (new_mod_list)
            snd_use_case_free_list(new_mod_list, new_mod_count);
    }

    return audio_ret;
}

audio_return_t _audio_ucm_set_devices(audio_hal_t *ah, const char *verb, const char *devices[])
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    int is_verb_changed = 0, is_dev_changed = 0;
    const char *old_verb = NULL, **old_dev_list = NULL;
    int old_dev_count = 0, dev_count = 0;
    const char **dis_dev_list = NULL, **ena_dev_list = NULL;
    int dis_dev_count = 0, ena_dev_count = 0;
    int i = 0, j = 0;
    char dump_str[512];

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(ah->ucm.uc_mgr, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(verb, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(devices, AUDIO_ERR_PARAMETER);

    snd_use_case_get(ah->ucm.uc_mgr, "_verb", &old_verb);
    old_dev_count = snd_use_case_get_list(ah->ucm.uc_mgr, "_enadevs", &old_dev_list);
    __dump_use_case(old_verb, old_dev_list, old_dev_count, NULL, 0, &dump_str[0]);
    AUDIO_LOG_INFO(">>> UCM current %s", dump_str);

    if (devices) {
        for (dev_count = 0; devices[dev_count]; dev_count++);
    }

    __dump_use_case(verb, devices, dev_count, NULL, 0, &dump_str[0]);
    AUDIO_LOG_INFO("> UCM requested %s", dump_str);

    if (old_verb && streq(verb, old_verb)) {
        AUDIO_LOG_DEBUG("current verb and new verb is same. No need to change verb, disable devices explicitely");

        if (old_dev_count > 0) {
            dis_dev_list = (const char **)malloc(sizeof(const char *) * old_dev_count);
            for (i = 0; i < old_dev_count; i++) {
                dis_dev_list[i] = NULL;
            }
        }
        if (dev_count > 0) {
            ena_dev_list = (const char **)malloc(sizeof(const char *) * dev_count);
            for (i = 0; i < dev_count; i++) {
                ena_dev_list[i] = NULL;
            }
        }

        /* update disable devices list which are not present in new device list */
        for (i = 0; i < old_dev_count; i++) {
            int need_disable_dev = 1;

            for (j = 0; j < dev_count; j++) {
                if (streq(old_dev_list[i], devices[j])) {
                    need_disable_dev = 0;
                    break;
                }
            }
            if (need_disable_dev) {
                if (is_dev_changed == 0)
                    is_dev_changed = 1;
                dis_dev_list[dis_dev_count++] = old_dev_list[i];
            }
        }

        /* update enable devices list which are not present in old device list */
        for (i = 0; i < dev_count; i++) {
            int need_enable_dev = 1;

            for (j = 0; j < old_dev_count; j++) {
                if (streq(devices[i], old_dev_list[j])) {
                    need_enable_dev = 0;
                    break;
                }
            }
            if (need_enable_dev) {
                if (is_dev_changed == 0)
                    is_dev_changed = 1;
                ena_dev_list[ena_dev_count++] = devices[i];
            }
        }

        /* disable devices */
        for (i = 0; i < dis_dev_count; i++) {
            AUDIO_LOG_INFO("Disable device : %s", dis_dev_list[i]);
            if (snd_use_case_set(ah->ucm.uc_mgr, "_disdev", dis_dev_list[i]) < 0)
                AUDIO_LOG_ERROR("disable %s device failed", dis_dev_list[i]);
        }

        /* enable devices */
        for (i = 0; i < ena_dev_count; i++) {
            AUDIO_LOG_INFO("Enable device : %s", ena_dev_list[i]);
            if (snd_use_case_set(ah->ucm.uc_mgr, "_enadev", ena_dev_list[i]) < 0)
                AUDIO_LOG_ERROR("enable %s device failed", ena_dev_list[i]);
        }

    } else {
        is_verb_changed = 1;

        AUDIO_LOG_DEBUG("Setting new verb: %s", verb);
        /* set new verb */
        if (snd_use_case_set(ah->ucm.uc_mgr, "_verb", verb) < 0) {
            AUDIO_LOG_ERROR("Setting verb %s failed", verb);
            audio_ret = AUDIO_ERR_UNDEFINED;
            goto exit;
        }
        /* enable devices */
        for (i = 0; i < dev_count; i++) {
            AUDIO_LOG_DEBUG("Enable device : %s", devices[i]);
            if(snd_use_case_set(ah->ucm.uc_mgr, "_enadev", devices[i]) < 0)
                AUDIO_LOG_ERROR("Enable %s device failed", devices[i]);
        }
    }

exit:
    if (old_verb)
        free((void *)old_verb);
    if (old_dev_list)
        snd_use_case_free_list(old_dev_list, old_dev_count);
    if (dis_dev_list)
        free((void *)dis_dev_list);
    if (ena_dev_list)
        free((void *)ena_dev_list);

    if (is_verb_changed == 1 || is_dev_changed == 1) {
        const char *new_verb = NULL, **new_dev_list = NULL;
        int new_dev_count = 0;

        snd_use_case_get(ah->ucm.uc_mgr, "_verb", &new_verb);
        new_dev_count = snd_use_case_get_list(ah->ucm.uc_mgr, "_enadevs", &new_dev_list);
        __dump_use_case(new_verb, new_dev_list, new_dev_count, NULL, 0, &dump_str[0]);
        AUDIO_LOG_INFO("<<< UCM changed %s", dump_str);

        if (new_verb)
            free((void *)new_verb);
        if (new_dev_list)
            snd_use_case_free_list(new_dev_list, new_dev_count);
    }

    return audio_ret;

}

audio_return_t _audio_ucm_set_modifiers(audio_hal_t *ah, const char *verb, const char *modifiers[])
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    int is_verb_changed = 0, is_mod_changed = 0;
    const char *old_verb = NULL, **old_mod_list = NULL;
    int old_mod_count = 0, mod_count = 0;
    const char **dis_mod_list = NULL, **ena_mod_list = NULL;
    int dis_mod_count = 0, ena_mod_count = 0;
    int i = 0, j = 0;
    char dump_str[512];

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(ah->ucm.uc_mgr, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(verb, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(modifiers, AUDIO_ERR_PARAMETER);

    snd_use_case_get(ah->ucm.uc_mgr, "_verb", &old_verb);
    old_mod_count = snd_use_case_get_list(ah->ucm.uc_mgr, "_enamods", &old_mod_list);
    __dump_use_case(old_verb, NULL, 0, old_mod_list, old_mod_count, &dump_str[0]);
    AUDIO_LOG_INFO(">>> UCM current %s", dump_str);

    if (modifiers) {
        for (mod_count = 0; modifiers[mod_count]; mod_count++);
    }

    __dump_use_case(verb, NULL, 0, modifiers, mod_count, &dump_str[0]);
    AUDIO_LOG_INFO("> UCM requested %s", dump_str);

    if (old_verb && streq(verb, old_verb)) {
        AUDIO_LOG_DEBUG("current verb and new verb is same. No need to change verb, disable devices explicitely");

        if (old_mod_count > 0) {
            dis_mod_list = (const char **)malloc(sizeof(const char *) * old_mod_count);
            for (i = 0; i < old_mod_count; i++) {
                dis_mod_list[i] = NULL;
            }
        }
        if (mod_count > 0) {
            ena_mod_list = (const char **)malloc(sizeof(const char *) * mod_count);
            for (i = 0; i < mod_count; i++) {
                ena_mod_list[i] = NULL;
            }
        }

        /* update disable modifiers list which are not present in new modifier list */
        for (i = 0; i < old_mod_count; i++) {
            int need_disable_mod = 1;

            for (j = 0; j < mod_count; j++) {
                if (streq(old_mod_list[i], modifiers[j])) {
                    need_disable_mod = 0;
                    break;
                }
            }
            if (need_disable_mod) {
                if (is_mod_changed == 0)
                    is_mod_changed = 1;
                dis_mod_list[dis_mod_count++] = old_mod_list[i];
            }
        }

        /* update enable modifiers list which are not present in old modifier list */
        for (i = 0; i < mod_count; i++) {
            int need_enable_mod = 1;

            for (j = 0; j < old_mod_count; j++) {
                if (streq(modifiers[i], old_mod_list[j])) {
                    need_enable_mod = 0;
                    break;
                }
            }
            if (need_enable_mod) {
                if (is_mod_changed == 0)
                    is_mod_changed = 1;
                ena_mod_list[ena_mod_count++] = modifiers[i];
            }
        }

        /* disable modifiers */
        for (i = 0; i < dis_mod_count; i++) {
            AUDIO_LOG_INFO("Disable modifier : %s", dis_mod_list[i]);
            if (snd_use_case_set(ah->ucm.uc_mgr, "_dismod", dis_mod_list[i]) < 0)
                AUDIO_LOG_ERROR("disable %s modifier failed", dis_mod_list[i]);
        }

        /* enable modifiers */
        for (i = 0; i < ena_mod_count; i++) {
            AUDIO_LOG_INFO("Enable modifier : %s", ena_mod_list[i]);
            if (snd_use_case_set(ah->ucm.uc_mgr, "_enamod", ena_mod_list[i]) < 0)
                AUDIO_LOG_ERROR("enable %s modifier failed", ena_mod_list[i]);
        }
    } else {
        is_verb_changed = 1;

        AUDIO_LOG_DEBUG("Setting new verb: %s", verb);
        /* set new verb */
        if (snd_use_case_set(ah->ucm.uc_mgr, "_verb", verb) < 0) {
            AUDIO_LOG_ERROR("Setting verb %s failed", verb);
            audio_ret = AUDIO_ERR_UNDEFINED;
            goto exit;
        }
        /* enable modifiers */
        for (i = 0; i < mod_count; i++) {
            AUDIO_LOG_DEBUG("Enable modifier : %s", modifiers[i]);
            if(snd_use_case_set(ah->ucm.uc_mgr, "_enamod", modifiers[i]) < 0)
                AUDIO_LOG_ERROR("Enable %s modifier failed", modifiers[i]);
        }
    }

exit:
    if (old_verb)
        free((void *)old_verb);
    if (old_mod_list)
        snd_use_case_free_list(old_mod_list, old_mod_count);
    if (dis_mod_list)
        free((void *)dis_mod_list);
    if (ena_mod_list)
        free((void *)ena_mod_list);

    if (is_verb_changed == 1 || is_mod_changed == 1) {
        const char *new_verb = NULL, **new_mod_list = NULL;
        int new_mod_count = 0;

        snd_use_case_get(ah->ucm.uc_mgr, "_verb", &new_verb);
        new_mod_count = snd_use_case_get_list(ah->ucm.uc_mgr, "_enamods", &new_mod_list);
        __dump_use_case(new_verb, NULL, 0, new_mod_list, new_mod_count, &dump_str[0]);
        AUDIO_LOG_INFO("<<< UCM changed %s", dump_str);

        if (new_verb)
            free((void *)new_verb);
        if (new_mod_list)
            snd_use_case_free_list(new_mod_list, new_mod_count);
    }

    return audio_ret;
}

audio_return_t _audio_ucm_get_verb(audio_hal_t *ah, const char **value)
{
    audio_return_t ret = AUDIO_RET_OK;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(ah->ucm.uc_mgr, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(value, AUDIO_ERR_PARAMETER);

    if ((ret = snd_use_case_get(ah->ucm.uc_mgr, "_verb", value)) < 0) {
        AUDIO_LOG_ERROR("Getting current verb failed: Reason %d", ret);
        ret = AUDIO_ERR_UNDEFINED;
    }

    return ret;
}


audio_return_t _audio_ucm_reset_use_case(audio_hal_t *ah)
{
    audio_return_t ret = AUDIO_RET_OK;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(ah->ucm.uc_mgr, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO(">>> UCM reset Verb [ %s ]", AUDIO_USE_CASE_VERB_INACTIVE);

    if ((ret = snd_use_case_set(ah->ucm.uc_mgr, "_verb", AUDIO_USE_CASE_VERB_INACTIVE)) < 0) {
        AUDIO_LOG_ERROR("Reset use case failed: Reason %d", ret);
        ret = AUDIO_ERR_UNDEFINED;
    }

    return ret;
}

