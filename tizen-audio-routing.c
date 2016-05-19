/*
 * audio-hal
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd. All rights reserved.
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
#include <stdbool.h>

#include "tizen-audio-internal.h"
#include "tizen-audio-impl.h"

/* #define DEBUG_TIMING */

static device_type_t outDeviceTypes[] = {
    { AUDIO_DEVICE_OUT_SPEAKER, "Speaker" },
    { AUDIO_DEVICE_OUT_RECEIVER, "Earpiece" },
    { AUDIO_DEVICE_OUT_JACK, "Headphones" },
    { AUDIO_DEVICE_OUT_BT_SCO, "Bluetooth" },
    { 0, 0 },
};

static device_type_t inDeviceTypes[] = {
    { AUDIO_DEVICE_IN_MAIN_MIC, "MainMic" },
    { AUDIO_DEVICE_IN_SUB_MIC, "SubMic" },
    { AUDIO_DEVICE_IN_JACK, "HeadsetMic" },
    { AUDIO_DEVICE_IN_BT_SCO, "BT Mic" },
    { 0, 0 },
};

static const char* mode_to_verb_str[] = {
    AUDIO_USE_CASE_VERB_HIFI,
    AUDIO_USE_CASE_VERB_VOICECALL,
    AUDIO_USE_CASE_VERB_VIDEOCALL,
    AUDIO_USE_CASE_VERB_VOIP,
};

static uint32_t __convert_device_string_to_enum(const char* device_str, uint32_t direction)
{
    uint32_t device = 0;

    if (!strncmp(device_str, "builtin-speaker", MAX_NAME_LEN)) {
        device = AUDIO_DEVICE_OUT_SPEAKER;
    } else if (!strncmp(device_str, "builtin-receiver", MAX_NAME_LEN)) {
        device = AUDIO_DEVICE_OUT_RECEIVER;
    } else if ((!strncmp(device_str, "audio-jack", MAX_NAME_LEN)) && (direction == AUDIO_DIRECTION_OUT)) {
        device = AUDIO_DEVICE_OUT_JACK;
    } else if ((!strncmp(device_str, "bt", MAX_NAME_LEN)) && (direction == AUDIO_DIRECTION_OUT)) {
        device = AUDIO_DEVICE_OUT_BT_SCO;
    } else if ((!strncmp(device_str, "builtin-mic", MAX_NAME_LEN))) {
        device = AUDIO_DEVICE_IN_MAIN_MIC;
    /* To Do : SUB_MIC */
    } else if ((!strncmp(device_str, "audio-jack", MAX_NAME_LEN)) && (direction == AUDIO_DIRECTION_IN)) {
        device = AUDIO_DEVICE_IN_JACK;
    } else if ((!strncmp(device_str, "bt", MAX_NAME_LEN)) && (direction == AUDIO_DIRECTION_IN)) {
        device = AUDIO_DEVICE_IN_BT_SCO;
    } else {
        device = AUDIO_DEVICE_NONE;
    }
    AUDIO_LOG_INFO("device type(%s), enum(0x%x)", device_str, device);
    return device;
}

static void __reset_voice_devices_info(audio_hal_t *ah)
{
    AUDIO_RETURN_IF_FAIL(ah);

    AUDIO_LOG_INFO("reset voice device info");
    if (ah->device.init_call_devices) {
        free(ah->device.init_call_devices);
        ah->device.init_call_devices = NULL;
        ah->device.num_of_call_devices = 0;
    }

    return;
}

static audio_return_t __set_devices(audio_hal_t *ah, const char *verb, device_info_t *devices, uint32_t num_of_devices)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    uint32_t new_device = 0;
    const char *active_devices[MAX_DEVICES] = {NULL,};
    int i = 0, j = 0, dev_idx = 0;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(devices, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(num_of_devices, AUDIO_ERR_PARAMETER);

    if (num_of_devices > MAX_DEVICES) {
        num_of_devices = MAX_DEVICES;
        AUDIO_LOG_ERROR("error: num_of_devices");
        return AUDIO_ERR_PARAMETER;
    }

    if (devices[0].direction == AUDIO_DIRECTION_OUT) {
        ah->device.active_out &= 0x0;
        if (ah->device.active_in) {
            /* check the active in devices */
            for (j = 0; j < inDeviceTypes[j].type; j++) {
                if (((ah->device.active_in & (~AUDIO_DEVICE_IN)) & inDeviceTypes[j].type))
                    active_devices[dev_idx++] = inDeviceTypes[j].name;
            }
        }
    } else if (devices[0].direction == AUDIO_DIRECTION_IN) {
        ah->device.active_in &= 0x0;
        if (ah->device.active_out) {
            /* check the active out devices */
            for (j = 0; j < outDeviceTypes[j].type; j++) {
                if (ah->device.active_out & outDeviceTypes[j].type)
                    active_devices[dev_idx++] = outDeviceTypes[j].name;
            }
        }
    }

    for (i = 0; i < num_of_devices; i++) {
        new_device = __convert_device_string_to_enum(devices[i].type, devices[i].direction);
        if (new_device & AUDIO_DEVICE_IN) {
            for (j = 0; j < inDeviceTypes[j].type; j++) {
                if (new_device == inDeviceTypes[j].type) {
                    active_devices[dev_idx++] = inDeviceTypes[j].name;
                    ah->device.active_in |= new_device;
                }
            }
        } else {
            for (j = 0; j < outDeviceTypes[j].type; j++) {
                if (new_device == outDeviceTypes[j].type) {
                    active_devices[dev_idx++] = outDeviceTypes[j].name;
                    ah->device.active_out |= new_device;
                }
            }
        }
    }

    if (active_devices[0] == NULL) {
        AUDIO_LOG_ERROR("Failed to set device: active device is NULL");
        return AUDIO_ERR_PARAMETER;
    }

    audio_ret = _ucm_set_devices(ah, verb, active_devices);
    if (audio_ret)
        AUDIO_LOG_ERROR("Failed to set device: error = %d", audio_ret);

    return audio_ret;
}

static audio_return_t __update_route_ap_playback_capture(audio_hal_t *ah, audio_route_info_t *route_info)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    device_info_t *devices = NULL;
    const char *verb = mode_to_verb_str[VERB_NORMAL];
#if 0  /* Disable setting modifiers, because driver does not support it yet */
    int mod_idx = 0;
    const char *modifiers[MAX_MODIFIERS] = {NULL,};
#endif

    if (ah->modem.is_connected) {
        AUDIO_LOG_INFO("modem is connected, skip verb[%s]", verb);
        return audio_ret;
    }

    if (ah->device.mode != VERB_NORMAL) {
        if (ah->device.mode == VERB_VOICECALL) {
            __reset_voice_devices_info(ah);
            COND_SIGNAL(ah->device.device_cond, "device_cond");
        }
        _reset_pcm_devices(ah);
        ah->device.mode = VERB_NORMAL;
    }

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(route_info, AUDIO_ERR_PARAMETER);

    devices = route_info->device_infos;

    AUDIO_LOG_INFO("update_route_ap_playback_capture++ ");

    audio_ret = __set_devices(ah, verb, devices, route_info->num_of_devices);
    if (audio_ret) {
        AUDIO_LOG_ERROR("Failed to set devices: error = 0x%x", audio_ret);
        return audio_ret;
    }
    ah->device.mode = VERB_NORMAL;

#if 0 /* Disable setting modifiers, because driver does not support it yet */
    /* Set modifiers */
    if (!strncmp("voice_recognition", route_info->role, MAX_NAME_LEN)) {
        modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_VOICESEARCH;
    } else if ((!strncmp("alarm", route_info->role, MAX_NAME_LEN))||(!strncmp("notifiication", route_info->role, MAX_NAME_LEN))) {
        if (ah->device.active_out &= AUDIO_DEVICE_OUT_JACK)
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_DUAL_MEDIA;
        else
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_MEDIA;
    } else if (!strncmp("ringtone", route_info->role, MAX_NAME_LEN)) {
        if (ah->device.active_out)
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_RINGTONE;
    } else {
        if (ah->device.active_in)
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_CAMCORDING;
        else
            modifiers[mod_idx++] = AUDIO_USE_CASE_MODIFIER_MEDIA;
    }
    audio_ret = _audio_ucm_set_modifiers (ah, verb, modifiers);
#endif
    return audio_ret;
}

static audio_return_t __update_route_voicecall(audio_hal_t *ah, device_info_t *devices, int32_t num_of_devices)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    const char *verb = mode_to_verb_str[VERB_VOICECALL];

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    /* if both params are 0, return error for invalid state,
         * this error would be used to tizen-audio-modem.c */
    AUDIO_RETURN_VAL_IF_FAIL((devices||num_of_devices), AUDIO_ERR_INVALID_STATE);
    AUDIO_RETURN_VAL_IF_FAIL(devices, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("update_route_voicecall++");

    audio_ret = __set_devices(ah, verb, devices, num_of_devices);
    if (audio_ret) {
        AUDIO_LOG_ERROR("Failed to set devices: error = 0x%x", audio_ret);
        return audio_ret;
    }

    if (ah->device.mode != VERB_VOICECALL) {
        _voice_pcm_open(ah);
        ah->device.mode = VERB_VOICECALL;
        /* FIXME. Get network info and configure rate in pcm device */
    }

    return audio_ret;
}

static audio_return_t __update_route_voip(audio_hal_t *ah, device_info_t *devices, int32_t num_of_devices)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    const char *verb = mode_to_verb_str[VERB_NORMAL];

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(devices, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("update_route_voip++");

    audio_ret = __set_devices(ah, verb, devices, num_of_devices);
    if (audio_ret) {
        AUDIO_LOG_ERROR("Failed to set devices: error = 0x%x", audio_ret);
        return audio_ret;
    }
    /* FIXME. If necessary, set VERB_VOIP */
    ah->device.mode = VERB_NORMAL;

    /* TO DO: Set modifiers */
    return audio_ret;
}

static audio_return_t __update_route_reset(audio_hal_t *ah, uint32_t direction)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    const char *active_devices[MAX_DEVICES] = {NULL,};
    int i = 0, dev_idx = 0;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("update_route_reset++, direction(0x%x)", direction);

    if (direction == AUDIO_DIRECTION_OUT) {
        ah->device.active_out &= 0x0;
        if (ah->device.active_in) {
            /* check the active in devices */
            for (i = 0; i < inDeviceTypes[i].type; i++) {
                if (((ah->device.active_in & (~AUDIO_DEVICE_IN)) & inDeviceTypes[i].type)) {
                    active_devices[dev_idx++] = inDeviceTypes[i].name;
                    AUDIO_LOG_INFO("added for in : %s", inDeviceTypes[i].name);
                }
            }
        }
    } else {
        ah->device.active_in &= 0x0;
        if (ah->device.active_out) {
            /* check the active out devices */
            for (i = 0; i < outDeviceTypes[i].type; i++) {
                if (ah->device.active_out & outDeviceTypes[i].type) {
                    active_devices[dev_idx++] = outDeviceTypes[i].name;
                    AUDIO_LOG_INFO("added for out : %s", outDeviceTypes[i].name);
                }
            }
        }
    }
    if (ah->device.mode == VERB_VOICECALL) {
        _voice_pcm_close(ah, direction);
        if (!ah->device.active_in && !ah->device.active_out)
            ah->device.mode = VERB_NORMAL;
        __reset_voice_devices_info(ah);
        COND_SIGNAL(ah->device.device_cond, "device_cond");
    }

    if (active_devices[0] == NULL) {
        AUDIO_LOG_DEBUG("active device is NULL, no need to update.");
        return AUDIO_RET_OK;
    }

    if ((audio_ret = _ucm_set_devices(ah, mode_to_verb_str[ah->device.mode], active_devices)))
        AUDIO_LOG_ERROR("failed to _ucm_set_devices(), ret(0x%x)", audio_ret);

    return audio_ret;
}

audio_return_t _audio_update_route_voicecall(audio_hal_t *ah, device_info_t *devices, int32_t num_of_devices)
{
    return __update_route_voicecall(ah, devices, num_of_devices);
}

audio_return_t _audio_routing_init(audio_hal_t *ah)
{
    audio_return_t audio_ret = AUDIO_RET_OK;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);

    ah->device.active_in = 0x0;
    ah->device.active_out = 0x0;
    ah->device.mode = VERB_NORMAL;

    if ((audio_ret = _ucm_init(ah)))
        AUDIO_LOG_ERROR("failed to _ucm_init(), ret(0x%x)", audio_ret);

    return audio_ret;
}

audio_return_t _audio_routing_deinit(audio_hal_t *ah)
{
    audio_return_t audio_ret = AUDIO_RET_OK;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);

    if ((audio_ret = _ucm_deinit(ah)))
        AUDIO_LOG_ERROR("failed to _ucm_deinit(), ret(0x%x)", audio_ret);

    return audio_ret;
}

audio_return_t audio_update_route(void *audio_handle, audio_route_info_t *info)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_hal_t *ah = (audio_hal_t *)audio_handle;
    device_info_t *devices = NULL;
    uint32_t prev_size;
    int32_t i;
    int32_t j;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(info, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("role:%s", info->role);

    devices = info->device_infos;

    if (!strncmp("call-voice", info->role, MAX_NAME_LEN)) {
        if (!ah->modem.is_connected) {
            if (info->num_of_devices) {
                if (!ah->device.num_of_call_devices) {
                    if ((ah->device.init_call_devices = (device_info_t*)calloc(info->num_of_devices, sizeof(device_info_t)))) {
                        memcpy(ah->device.init_call_devices, devices, info->num_of_devices*sizeof(device_info_t));
                        ah->device.num_of_call_devices = info->num_of_devices;
                    } else {
                        AUDIO_LOG_ERROR("failed to calloc");
                        audio_ret = AUDIO_ERR_RESOURCE;
                        goto ERROR;
                    }
                } else if (ah->device.num_of_call_devices) {
                    prev_size = ah->device.num_of_call_devices;
                    if (prev_size == 2) {
                        /* There's a chance to be requested to change routing from user
                         * though two devices(for input/output) has already been set for call-voice routing.
                         * In this case, exchange an old device for a new device if it's direction is same as an old one's. */
                        for (i = 0; i < prev_size; i++) {
                            for (j = 0; j < info->num_of_devices; j++) {
                                if (devices[j].direction == ah->device.init_call_devices[i].direction &&
                                    devices[j].id != ah->device.init_call_devices[i].id)
                                    memcpy(&ah->device.init_call_devices[i], &devices[j], sizeof(device_info_t));
                            }
                        }
                    } else if (prev_size < 2) {
                        /* A device has already been added for call-voice routing,
                         * and now it is about to add a new device(input or output device). */
                        ah->device.num_of_call_devices += info->num_of_devices;
                        if ((ah->device.init_call_devices = (device_info_t*)realloc(ah->device.init_call_devices, sizeof(device_info_t)*ah->device.num_of_call_devices))) {
                            memcpy((void*)&(ah->device.init_call_devices[prev_size]), devices, info->num_of_devices*sizeof(device_info_t));
                        } else {
                            AUDIO_LOG_ERROR("failed to realloc");
                            audio_ret = AUDIO_ERR_RESOURCE;
                            goto ERROR;
                        }
                    } else {
                        AUDIO_LOG_ERROR("invaild previous num. of call devices");
                        audio_ret = AUDIO_ERR_INTERNAL;
                        goto ERROR;
                    }
                }
            } else {
                AUDIO_LOG_ERROR("failed to update route for call-voice, num_of_devices is 0");
                audio_ret = AUDIO_ERR_PARAMETER;
                goto ERROR;
            }
            AUDIO_LOG_INFO("modem is not ready, skip...");
        } else {
            if ((audio_ret = __update_route_voicecall(ah, devices, info->num_of_devices)))
                AUDIO_LOG_WARN("update voicecall route return 0x%x", audio_ret);

            COND_SIGNAL(ah->device.device_cond, "device_cond");
        }
    } else if (!strncmp("voip", info->role, MAX_NAME_LEN)) {
        if ((audio_ret = __update_route_voip(ah, devices, info->num_of_devices)))
            AUDIO_LOG_WARN("update voip route return 0x%x", audio_ret);

    } else if (!strncmp("reset", info->role, MAX_NAME_LEN)) {
        if ((audio_ret = __update_route_reset(ah, devices->direction)))
            AUDIO_LOG_WARN("update reset return 0x%x", audio_ret);

    } else {
        /* need to prepare for "alarm","notification","emergency","voice-information","voice-recognition","ringtone" */
        if ((audio_ret = __update_route_ap_playback_capture(ah, info)))
            AUDIO_LOG_WARN("update playback route return 0x%x", audio_ret);
    }

ERROR:
    return audio_ret;
}

audio_return_t audio_update_route_option(void *audio_handle, audio_route_option_t *option)
{
    audio_return_t audio_ret = AUDIO_RET_OK;
    audio_hal_t *ah = (audio_hal_t *)audio_handle;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);
    AUDIO_RETURN_VAL_IF_FAIL(option, AUDIO_ERR_PARAMETER);

    AUDIO_LOG_INFO("role:%s, name:%s, value:%d", option->role, option->name, option->value);

    return audio_ret;
}