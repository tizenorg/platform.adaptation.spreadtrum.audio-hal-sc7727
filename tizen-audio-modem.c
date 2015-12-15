/*
 * audio-hal
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
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
#include <expat.h>
#include <stdbool.h>
#include <vconf.h>

#include "tizen-audio-internal.h"

#define vbc_thread_new pthread_create
#define MIXER_VBC_SWITCH                            "VBC Switch"
/* pin_switch */
#define PIN_SWITCH_IIS0_SYS_SEL              "IIS0 pin select"
#define PIN_SWITCH_IIS0_AP_ID                0
#define PIN_SWITCH_IIS0_CP0_ID               1
#define PIN_SWITCH_IIS0_CP1_ID               2
#define PIN_SWITCH_IIS0_CP2_ID               3
#define PIN_SWITCH_IIS0_VBC_ID               4

#define PIN_SWITCH_BT_IIS_SYS_SEL            "BT IIS pin select"
#define PIN_SWITCH_BT_IIS_CP0_IIS0_ID        0
#define PIN_SWITCH_BT_IIS_CP1_IIS0_ID        4
#define PIN_SWITCH_BT_IIS_AP_IIS0_ID         8

#define PIN_SWITCH_BT_IIS_CON_SWITCH         "BT IIS con switch"

#define VBC_TD_CHANNELID                            0  /*  cp [3g] */
#define VBC_ARM_CHANNELID                           2  /*  ap */

#define VBPIPE_DEVICE           "/dev/spipe_w6"
#define VBPIPE_VOIP_DEVICE      "/dev/spipe_w4"
#define VBC_CMD_TAG             "VBC"

/* Voice */
typedef enum {
    VBC_CMD_NONE = 0,
    /* current mode and volume gain parameters.*/
    VBC_CMD_SET_MODE = 1,
    VBC_CMD_RESP_MODE = 2,

    VBC_CMD_SET_GAIN = 3,
    VBC_CMD_RESP_GAIN = 4,

    /* whether switch vb control to dsp parameters.*/
    VBC_CMD_SWITCH_CTRL = 5,
    VBC_CMD_RESP_SWITCH = 6,

    /* whether mute or not.*/
    VBC_CMD_SET_MUTE = 7,
    VBC_CMD_RESP_MUTE = 8,

    /* open/close device parameters.*/
    VBC_CMD_DEVICE_CTRL = 9,
    VBC_CMD_RESP_DEVICE = 10,

    VBC_CMD_PCM_OPEN = 11,
    VBC_CMD_RESP_OPEN  =12,

    VBC_CMD_PCM_CLOSE = 13,
    VBC_CMD_RESP_CLOSE = 14,

    VBC_CMD_SET_SAMPLERATE = 15,
    VBC_CMD_RESP_SAMPLERATE = 16,

    VBC_CMD_MAX
} vbc_command;

const static char* vbc_cmd_str_arry[VBC_CMD_MAX] = {"NONE", "SET_MODE", "RESP_MODE", "SET_GAIN", "RESP_GAIN", "SWITCH_CTL", "RESP_SWITCH",
"SET_MUTE", "RESP_MUTE", "DEVICE_CTL", "RESP_DEVICE", "PCM_OPEN", "RESP_OPEN", "PCM_CLOSE", "RESP_CLOSE", "SET_SAMPL", "RESP_SAMPL"};

typedef struct {
    unsigned int  sim_card;   /*sim card number*/
} open_pcm_t;

typedef struct _vbc_parameters_head {
    char            tag[4];
    unsigned int    cmd_type;
    unsigned int    paras_size;
} vbc_parameters_head;

typedef struct vbc_control_params {
    int vbchannel_id;
    audio_hal_t *ah;
} vbc_control_params_t;

typedef struct {
    unsigned short      is_open; /* if is_open is true, open device; else close device.*/
    unsigned short      is_headphone;
    unsigned int        is_downlink_mute;
    unsigned int        is_uplink_mute;
} device_ctrl_t;

typedef struct samplerate_ctrl {
    unsigned int samplerate; /* change samplerate.*/
} set_samplerate_t;

typedef struct {
    unsigned int  is_switch; /* switch vbc contrl to dsp.*/
} switch_ctrl_t;

static int __read_nonblock(int fd, void *buf, int bytes)
{
    int ret = 0;
    int bytes_to_read = bytes;

    if ((fd > 0) && (buf != NULL)) {
        do {
            ret = read(fd, buf, bytes);
            if ( ret > 0) {
                if (ret <= bytes) {
                    bytes -= ret;
                }
            } else if ((!((errno == EAGAIN) || (errno == EINTR))) || (0 == ret)) {
                break;
            }
        } while(bytes);
    }

    if (bytes == bytes_to_read)
        return ret ;
    else
        return (bytes_to_read - bytes);
}

static int __write_nonblock(int fd, void *buf, int bytes)
{
    int ret = -1;
    int bytes_to_write = bytes;

    if ((fd > 0) && (buf != NULL)) {
        do {
            ret = write(fd, buf, bytes);
            if ( ret > 0) {
                if (ret <= bytes) {
                    bytes -= ret;
                }
            } else if ((!((errno == EAGAIN) || (errno == EINTR))) || (0 == ret)) {
                break;
            }
        } while(bytes);
    }

    if (bytes == bytes_to_write)
        return ret ;
    else
        return (bytes_to_write - bytes);

}

int _audio_modem_is_call_connected(audio_hal_t *ah)
{
    int val = -1; /* Mixer values 0 - cp [3g] ,1 - cp [2g] ,2 - ap */

    _audio_mixer_control_get_value(ah, MIXER_VBC_SWITCH, &val);

    return (val == VBC_TD_CHANNELID) ? 1 : 0;
}

static int __voice_read_samplerate(int fd, set_samplerate_t *paras_ptr)
{
    int ret = 0;
    if (fd > 0 && paras_ptr != NULL) {
    ret = __read_nonblock(fd, paras_ptr, sizeof(set_samplerate_t));
        if (ret != sizeof(set_samplerate_t))
            ret = -1;
    }
    AUDIO_LOG_INFO("Return value of read sample rate = %d", ret);
    return ret;

}

static int __voice_get_samplerate(audio_hal_t *ah, int fd)
{
    set_samplerate_t samplerate_paras;

    memset(&samplerate_paras, 0, sizeof(set_samplerate_t));
    __voice_read_samplerate(fd, &samplerate_paras);

    if (samplerate_paras.samplerate <= 0)
        ah->modem.samplerate = 8000;
    else
        ah->modem.samplerate = samplerate_paras.samplerate;

    return 0;
}

static int __vbc_write_response(int fd, unsigned int cmd, uint32_t paras_size)
{
    int ret = 0;
    vbc_parameters_head write_head;

    memset(&write_head, 0, sizeof(vbc_parameters_head));
    memcpy(&write_head.tag[0], VBC_CMD_TAG, 3);
    write_head.cmd_type = cmd + 1;
    write_head.paras_size = paras_size;

    ret = __write_nonblock(fd, (void*)&write_head, sizeof(vbc_parameters_head));
    if (ret < 0)
        AUDIO_LOG_ERROR("write failed");
    else
        AUDIO_LOG_DEBUG("write success for VBC_CMD_[%s]", vbc_cmd_str_arry[cmd]);

    return 0;
}

#define FM_IIS                                      0x10
static void i2s_pin_mux_sel(audio_hal_t *ah, int type)
{
    audio_return_t ret = AUDIO_RET_OK;
    audio_modem_t *modem;

    if (!ah) {
        AUDIO_LOG_ERROR("ah is null");
        return;
    }

    AUDIO_LOG_INFO("type is %d",type);
    modem = ah->modem.cp;

    if (type == FM_IIS) {
        _audio_mixer_control_set_value(ah,
                    PIN_SWITCH_IIS0_SYS_SEL, PIN_SWITCH_IIS0_VBC_ID);
        return;
    }
    if (type == 0) {
       if(ah->device.active_out & AUDIO_DEVICE_OUT_BT_SCO) {
            if(modem->i2s_bt.is_ext) {
                if(modem->i2s_bt.is_switch) {
                    ret = _audio_mixer_control_set_value(ah,
                            PIN_SWITCH_IIS0_SYS_SEL, PIN_SWITCH_IIS0_CP0_ID);
                }
            } else {
                if(modem->i2s_bt.is_switch) {
                    int value = 0;
                    _audio_mixer_control_get_value (ah, PIN_SWITCH_IIS0_SYS_SEL, &value);
                    if(value == PIN_SWITCH_IIS0_CP0_ID) {
                        ret = _audio_mixer_control_set_value(ah,
                                PIN_SWITCH_IIS0_SYS_SEL, PIN_SWITCH_IIS0_AP_ID);
                    }
                }
                if(ah->device.active_out & AUDIO_DEVICE_OUT_BT_SCO) {
                    if(modem->i2s_bt.is_switch) {
                        ret = _audio_mixer_control_set_value(ah,
                                PIN_SWITCH_BT_IIS_SYS_SEL, PIN_SWITCH_BT_IIS_CP0_IIS0_ID);
                    }
                }
            }
        }
    } else if (type == 1) {
        if(ah->device.active_out & AUDIO_DEVICE_OUT_BT_SCO) {
            if(modem->i2s_bt.is_ext) {
                if(modem->i2s_bt.is_switch) {
                    ret = _audio_mixer_control_set_value(ah,
                            PIN_SWITCH_IIS0_SYS_SEL, PIN_SWITCH_IIS0_CP1_ID);
                }
            } else {
                if(modem->i2s_bt.is_switch) {
                    int value = 0;
                    _audio_mixer_control_get_value (ah, PIN_SWITCH_IIS0_SYS_SEL, &value);
                    if(value == PIN_SWITCH_IIS0_CP1_ID) {
                        ret = _audio_mixer_control_set_value(ah,
                                PIN_SWITCH_IIS0_SYS_SEL, PIN_SWITCH_IIS0_CP2_ID);
                    }
                }
                if(ah->device.active_out & AUDIO_DEVICE_OUT_BT_SCO) {
                    if(modem->i2s_bt.is_switch) {
                        ret = _audio_mixer_control_set_value(ah,
                                PIN_SWITCH_BT_IIS_SYS_SEL, PIN_SWITCH_BT_IIS_CP1_IIS0_ID);
                    }
                }
            }
        }
    } else {
        AUDIO_LOG_ERROR("invalid type");
        return;
    }
    if (ret)
        AUDIO_LOG_ERROR("error(0x%x)", ret);

    return;
}

static void *__vbc_control_voice_thread_run(void *args)
{
    vbc_parameters_head read_head;
    vbc_parameters_head write_head;
    int exit_thread = 0; /* make exit variable global if required to gracefully exit */
    int vbpipe_fd;
    vbc_control_params_t *params = (vbc_control_params_t*)args;
    if (params == NULL) {
        return (void*)AUDIO_ERR_PARAMETER;
    }
    audio_hal_t *ah = params->ah;
    fd_set fds_read;
    struct timeval timeout = {5,0};

    memset(&read_head, 0, sizeof(vbc_parameters_head));
    memset(&write_head, 0, sizeof(vbc_parameters_head));

    memcpy(&write_head.tag[0], VBC_CMD_TAG, 3);
    write_head.cmd_type = VBC_CMD_NONE;
    write_head.paras_size = 0;

    AUDIO_LOG_INFO("[voice] vbc control VOICE thread run");

again:
    /* open vbpipe device for vb parameter interface between ap and cp */
    vbpipe_fd = open(VBPIPE_DEVICE, O_RDWR);
    if (vbpipe_fd < 0) {
        if (errno == EINTR)
            goto again;
        AUDIO_LOG_ERROR("[voice] vbpipe open failed: %s", strerror(errno));
        return (void*)AUDIO_ERR_IOCTL;
    }
    ah->modem.vbc.vbpipe_fd = vbpipe_fd;

    if (fcntl(vbpipe_fd, F_SETFL, O_NONBLOCK) < 0) {
        AUDIO_LOG_DEBUG("[voice] vbpipe_fd(%d) fcntl error.", vbpipe_fd);
    }

    /* initialize condition and mutex */
    pthread_cond_init(&(ah->modem.vbc.voice_thread_cond), NULL);
    pthread_mutex_init(&(ah->modem.vbc.voice_thread_lock), NULL);

    AUDIO_LOG_INFO("[voice] %s opened. vbc start loop", VBPIPE_DEVICE);

    /* start loop */
    while (!exit_thread) {
        int ret;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        if (!ah->modem.is_connected)
            MUTEX_LOCK(ah->modem.vbc.voice_thread_lock, "voice_thread_lock");

        /* read command received from cp */
        FD_ZERO(&fds_read);
        FD_SET(vbpipe_fd, &fds_read);

        ret = select(vbpipe_fd+1, &fds_read, NULL, NULL, &timeout);
        if (ret < 0) {
            ALOGE("voice:select error %d", errno);
            if (!ah->modem.is_connected)
                MUTEX_UNLOCK(ah->modem.vbc.voice_thread_lock, "voice_thread_lock");

            continue;
        }

        ret = __read_nonblock(vbpipe_fd, &read_head, sizeof(vbc_parameters_head));
        if (ret < 0) {
            if (!ah->modem.is_connected)
                MUTEX_UNLOCK(ah->modem.vbc.voice_thread_lock, "voice_thread_lock");

            continue;
        }

        AUDIO_LOG_DEBUG("[voice] Received %d bytes. data: %s, cmd_type: %d", ret, read_head.tag, read_head.cmd_type);

        if (!memcmp(&read_head.tag[0], VBC_CMD_TAG, 3)) {
            switch (read_head.cmd_type) {
                case VBC_CMD_PCM_OPEN: {
                    open_pcm_t open_pcm_params;
                    uint32_t paras_size = ((ah->modem.cp->i2s_bt.is_switch << 8) | (ah->modem.cp->i2s_bt.index << 0)
                                        | (ah->modem.cp->i2s_extspk.is_switch << 9) | (ah->modem.cp->i2s_extspk.index << 4));

                    AUDIO_LOG_INFO("[voice] Received VBC_CMD_PCM_OPEN");

                    ah->modem.is_connected = 1;

                    COND_SIGNAL(ah->modem.vbc.voice_thread_cond, "voice_thread_cond");
                    MUTEX_UNLOCK(ah->modem.vbc.voice_thread_lock, "voice_thread_lock");

                    /* wait for the ucm setting */
                    COND_WAIT(ah->device.device_cond, ah->device.device_lock, "device_cond");
                    MUTEX_UNLOCK(ah->device.device_lock, "device_lock");

                    memset(&open_pcm_params, 0, sizeof(open_pcm_t));
                    ret = __read_nonblock(vbpipe_fd, &open_pcm_params, sizeof(open_pcm_t));
                    if (ret < 0)
                        AUDIO_LOG_ERROR("read failed");
                    else
                        ah->modem.sim_id = open_pcm_params.sim_card;

                    if (ah->device.active_out & (AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_BT_SCO)) {
                        if (ah->modem.cp_type == CP_TG)
                            i2s_pin_mux_sel(ah, 1);
                        else if(ah->modem.cp_type == CP_W)
                            i2s_pin_mux_sel(ah, 0);
                    }

                    AUDIO_LOG_DEBUG("[voice] Send response for VBC_CMD_PCM_OPEN");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_PCM_OPEN, paras_size);
                    break;
                }

                case VBC_CMD_PCM_CLOSE: {
                    AUDIO_LOG_INFO("[voice] Received VBC_CMD_PCM_CLOSE");

                    ah->modem.samplerate = 0;

                    _audio_mixer_control_set_value(ah, MIXER_VBC_SWITCH, VBC_ARM_CHANNELID);

                    AUDIO_LOG_DEBUG("[voice] Send response for VBC_CMD_PCM_CLOSE");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_PCM_CLOSE, 0);
                    break;
                }

                case VBC_CMD_RESP_CLOSE: {
                    AUDIO_LOG_INFO("[voice] Received VBC_CMD_RESP_CLOSE & send response");
                    ret = __vbc_write_response(vbpipe_fd, VBC_CMD_PCM_CLOSE, 0);
                    ah->modem.is_connected = 0;
                    break;
                }

                case VBC_CMD_SET_MODE: {
                    char dummy[52];
                    memset(dummy, 0, sizeof(dummy));
                    AUDIO_LOG_INFO("[voice] Received VBC_CMD_SET_MODE");

                    if (ah->device.active_out & (AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_BT_SCO)) {
                        if (ah->modem.cp_type == CP_TG)
                            i2s_pin_mux_sel(ah, 1);
                        else if(ah->modem.cp_type == CP_W)
                            i2s_pin_mux_sel(ah, 0);
                    }
                    /* To do: set mode params : __vbc_set_mode_params(ah, vbpipe_fd); */

                    __read_nonblock(vbpipe_fd, dummy, sizeof(dummy));
                    AUDIO_LOG_DEBUG("[voice] Send response for VBC_CMD_SET_MODE");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_SET_MODE, 0);
                    break;
                }
                case VBC_CMD_SET_GAIN: {
                    AUDIO_LOG_INFO("[voice] Received VBC_CMD_SET_GAIN");

                    /* To do: set gain params : __vbc_set_gain_params(ah, vbpipe_fd); */

                    AUDIO_LOG_DEBUG("[voice] Send response for VBC_CMD_SET_GAIN");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_SET_GAIN, 0);
                    break;
                }
                case VBC_CMD_SWITCH_CTRL: {
                    switch_ctrl_t switch_ctrl_params;

                    AUDIO_LOG_INFO("[voice] Received VBC_CMD_SWITCH_CTRL");

                    memset(&switch_ctrl_params,0,sizeof(switch_ctrl_t));
                    ret = __read_nonblock(vbpipe_fd, &switch_ctrl_params, sizeof(switch_ctrl_t));
                    if (ret < 0)
                        AUDIO_LOG_ERROR("read failed");
                    else
                        AUDIO_LOG_INFO("is_switch:%d", switch_ctrl_params.is_switch);

                    _audio_mixer_control_set_value(ah, MIXER_VBC_SWITCH, VBC_TD_CHANNELID);

                    /* open pcm for virtual device */
                    //_open_virtual_device(ah);

                    AUDIO_LOG_DEBUG("[voice] Send response for VBC_CMD_SET_GAIN");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_SWITCH_CTRL, 0);
                   break;
                }
                case VBC_CMD_SET_MUTE: {
                    AUDIO_LOG_INFO("[voice] Received VBC_CMD_SET_MUTE");
                    break;
                }
                case VBC_CMD_DEVICE_CTRL: {
                    char dummy[64];
                    memset(dummy, 0, sizeof(dummy));
                    AUDIO_LOG_INFO("[voice] Received VBC_CMD_DEVICE_CTRL");

                    /* To do: set device ctrl params :__vbc_set_device_ctrl_params(ah, vbpipe_fd); */
                    __read_nonblock(vbpipe_fd, dummy, sizeof(dummy));

                    AUDIO_LOG_DEBUG("[voice] Send response for VBC_CMD_DEVICE_CTRL");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_DEVICE_CTRL, 0);

                    break;
                }
                case VBC_CMD_SET_SAMPLERATE: {
                    AUDIO_LOG_INFO("[voice] Received VBC_CMD_SET_SAMPLERATE");

//                    _voice_pcm_close(ah, 0);
//
//                    __voice_get_samplerate(ah, vbpipe_fd);
//
//                    ret = _voice_pcm_open(ah);
//                    if (ret < 0) {
//                        _voice_pcm_close(ah, 1);
//                        break;
//                    }

                    AUDIO_LOG_DEBUG("[voice] Send response for VBC_CMD_SET_SAMPLERATE");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_SET_SAMPLERATE, 0);
                    break;
                }
                default:
                    AUDIO_LOG_WARN("[voice] Unknown command received : %d", read_head.cmd_type);
                    break;
            }
        }
    }
    if (params)
        free(params);

    close(vbpipe_fd);

    /* de-initialize conditaion and mutex */
    pthread_cond_destroy(&(ah->modem.vbc.voice_thread_cond));
    pthread_mutex_destroy(&(ah->modem.vbc.voice_thread_lock));

    AUDIO_LOG_INFO("Exit vbc VOICE thread");

    return (void*)0;
}

static void *__vbc_control_voip_thread_run(void *args)
{
    open_pcm_t open_pcm_params;
    vbc_parameters_head read_head;
    vbc_parameters_head write_head;
    int exit_thread = 0; /* make exit variable global if required to gracefully exit */
    int vbpipe_fd;
    vbc_control_params_t *params = (vbc_control_params_t*)args;
    if (params == NULL) {
        return (void*)AUDIO_ERR_PARAMETER;
    }
    audio_hal_t *ah = params->ah;
    fd_set fds_read;

    struct timeval timeout = {5,0};

    memset(&read_head, 0, sizeof(vbc_parameters_head));
    memset(&write_head, 0, sizeof(vbc_parameters_head));

    memcpy(&write_head.tag[0], VBC_CMD_TAG, 3);
    write_head.cmd_type = VBC_CMD_NONE;
    write_head.paras_size = 0;

    AUDIO_LOG_INFO("[voip] vbc control VOIP thread run");

again:
    /* open vbpipe device for vb parameter interface between ap and cp */
    vbpipe_fd = open(VBPIPE_VOIP_DEVICE, O_RDWR);
    if (vbpipe_fd < 0) {
        if (errno == EINTR)
            goto again;
        AUDIO_LOG_ERROR("[voip] vbpipe open failed: %s", strerror(errno));
        return (void*)0;
    }
    ah->modem.vbc.vbpipe_voip_fd = vbpipe_fd;

    if (fcntl(vbpipe_fd, F_SETFL, O_NONBLOCK) < 0) {
        AUDIO_LOG_DEBUG("[voip] vbpipe_fd(%d) fcntl error.", vbpipe_fd);
    }

    AUDIO_LOG_INFO("[voip] %s opened. vbc start loop", VBPIPE_VOIP_DEVICE);

    /* start loop */
    while (!exit_thread) {
        int ret;
        timeout.tv_sec = 5;;
        timeout.tv_usec = 0;

        /* read command received from cp */

        FD_ZERO(&fds_read);
        FD_SET(vbpipe_fd, &fds_read);

        ret = select(vbpipe_fd+1, &fds_read, NULL, NULL, &timeout);
        if (ret < 0) {
            ALOGE("[voip] select error %d", errno);
            continue;
        }

        ret = __read_nonblock(vbpipe_fd, &read_head, sizeof(vbc_parameters_head));
        if (ret < 0) {
            continue;
        }

        AUDIO_LOG_DEBUG("[voip] Received %d bytes. data: %s, cmd_type: %d", ret, read_head.tag, read_head.cmd_type);

        if (!memcmp(&read_head.tag[0], VBC_CMD_TAG, 3)) {
            switch (read_head.cmd_type) {
                case VBC_CMD_PCM_OPEN: {
                    uint32_t paras_size = ((ah->modem.cp->i2s_bt.is_switch << 8) | (ah->modem.cp->i2s_bt.index << 0)
                                        | (ah->modem.cp->i2s_extspk.is_switch << 9) | (ah->modem.cp->i2s_extspk.index << 4));

                    AUDIO_LOG_INFO("[voip] Received VBC_CMD_PCM_OPEN");

                    memset(&open_pcm_params, 0, sizeof(open_pcm_t));
                    ret = __read_nonblock(vbpipe_fd, &open_pcm_params, sizeof(open_pcm_t));
                    if (ret < 0)
                        AUDIO_LOG_ERROR("read failed");

                    AUDIO_LOG_DEBUG("[voip] Send response for VBC_CMD_PCM_OPEN");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_PCM_OPEN, paras_size);
                    break;
                }
                case VBC_CMD_PCM_CLOSE: {
                    AUDIO_LOG_INFO("[voip] Received VBC_CMD_PCM_CLOSE & send response");

                    __vbc_write_response(vbpipe_fd, VBC_CMD_PCM_CLOSE, 0);

                    break;
                }
                case VBC_CMD_RESP_CLOSE: {
                    AUDIO_LOG_INFO("[voip] Received VBC_CMD_RESP_CLOSE & send response");

                    ret = __vbc_write_response(vbpipe_fd, VBC_CMD_PCM_CLOSE, 0);
                    break;
                }
                case VBC_CMD_SET_MODE: {
                    AUDIO_LOG_INFO("[voip] Received VBC_CMD_SET_MODE");

                    if (ah->device.active_out & (AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_BT_SCO)) {
                        if (ah->modem.cp_type == CP_TG)
                            i2s_pin_mux_sel(ah, 1);
                        else if(ah->modem.cp_type == CP_W)
                            i2s_pin_mux_sel(ah, 0);
                    }
                    /* To do: set mode params : __vbc_set_mode_params(ah, vbpipe_fd); */
                    AUDIO_LOG_DEBUG("[voip] Send response for VBC_CMD_SET_MODE");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_SET_MODE, 0);
                    break;
                }
                case VBC_CMD_SET_GAIN: {
                    AUDIO_LOG_INFO("[voip] Received VBC_CMD_SET_GAIN");

                    /* To do: set gain params : __vbc_set_gain_params(ah, vbpipe_fd); */
                    AUDIO_LOG_DEBUG("[voip] Send response for VBC_CMD_SET_GAIN");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_SET_GAIN, 0);
                    break;
                }
                case VBC_CMD_SWITCH_CTRL: {
                    switch_ctrl_t switch_ctrl_params;

                    AUDIO_LOG_INFO("[voip] Received VBC_CMD_SWITCH_CTRL");

                    memset(&switch_ctrl_params, 0, sizeof(switch_ctrl_t));
                    ret = __read_nonblock(vbpipe_fd, &switch_ctrl_params, sizeof(switch_ctrl_t));
                    if (ret < 0)
                        AUDIO_LOG_ERROR("read failed");
                    else
                        AUDIO_LOG_INFO("is_switch:%d", switch_ctrl_params.is_switch);

                    _audio_mixer_control_set_value(ah, MIXER_VBC_SWITCH, VBC_TD_CHANNELID);

                    AUDIO_LOG_DEBUG("[voip] Send response for VBC_CMD_SWITCH_CTRL");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_SWITCH_CTRL, 0);
                    break;
                }
                case VBC_CMD_SET_MUTE: {
                    AUDIO_LOG_INFO("[voip] Received VBC_CMD_SET_MUTE & send response");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_SET_MUTE, 0);
                    break;
                }
                case VBC_CMD_DEVICE_CTRL: {
                    AUDIO_LOG_INFO("[voip] Received VBC_CMD_DEVICE_CTRL");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_DEVICE_CTRL, 0);
                    break;
                }
                case VBC_CMD_SET_SAMPLERATE: {
                    AUDIO_LOG_INFO("[voip] Received VBC_CMD_SET_SAMPLERATE");

//                    _voice_pcm_close(ah, 0);
//                    __voice_get_samplerate(ah, vbpipe_fd);
//
//                    ret = _voice_pcm_open(ah);
//                    if (ret < 0) {
//                        _voice_pcm_close(ah, 1);
//                        break;
//                    }
                    AUDIO_LOG_DEBUG("[voip] Send response for VBC_CMD_SET_SAMPLERATE");
                    __vbc_write_response(vbpipe_fd, VBC_CMD_SET_SAMPLERATE, 0);
                    break;
                }
                default:
                    AUDIO_LOG_WARN("Unknown command received : %d", read_head.cmd_type);
                    break;
            }
        }
    }
    close(vbpipe_fd);
    if (params)
        free(params);

    AUDIO_LOG_INFO("Exit vbc VOIP thread");

    return (void*)0;
}

static audio_return_t __vbc_control_open(audio_hal_t *ah)
{
    vbc_control_params_t *params = (vbc_control_params_t*)malloc(sizeof(vbc_control_params_t));
    audio_return_t ret = AUDIO_RET_OK;
    audio_return_t ret2 = AUDIO_RET_OK;

    if (params == NULL) {
         AUDIO_LOG_ERROR("vbc control param allocation failed");
         return AUDIO_ERR_RESOURCE;
    }

    params->ah = ah;
    AUDIO_LOG_INFO("vbc control thread create");
    ret = vbc_thread_new(&ah->modem.vbc.voice_thread_handle, NULL, __vbc_control_voice_thread_run, (void*)params);
    if (ret < 0) {
        AUDIO_LOG_ERROR("vbc control thread create failed");
        ret = AUDIO_ERR_RESOURCE;
        return ret;
    }

    ret2 = vbc_thread_new(&ah->modem.vbc.voip_thread_handle, NULL, __vbc_control_voip_thread_run, (void*)params);
    if (ret2 < 0) {
        AUDIO_LOG_ERROR("vbc control VOIP thread create failed");
        ret2 = AUDIO_ERR_RESOURCE;
        return ret2;
    }

    return AUDIO_RET_OK;
}

static audio_return_t __vbc_control_close(audio_hal_t *ah)
{
    /* TODO. Make sure we always receive CLOSE command from modem and then close pcm device */
    ah->modem.vbc.exit_vbc_thread = 1;
    close(ah->modem.vbc.vbpipe_fd);

    pthread_cancel(ah->modem.vbc.voice_thread_handle);
    pthread_cancel(ah->modem.vbc.voip_thread_handle);

    return AUDIO_RET_OK;
}

static vbc_ctrl_pipe_para_t *__audio_modem_create(audio_modem_t  *modem, const char *num)
{
    if (!atoi((char *)num)) {
        AUDIO_LOG_ERROR("Unnormal modem num!");
        return NULL;
    }

    modem->num = atoi((char *)num);
    /* check if we need to allocate  space for modem profile */
    if(!modem->vbc_ctrl_pipe_info) {
        modem->vbc_ctrl_pipe_info = malloc(modem->num * sizeof(vbc_ctrl_pipe_para_t));

        if (modem->vbc_ctrl_pipe_info == NULL) {
            AUDIO_LOG_ERROR("Unable to allocate modem profiles");
            return NULL;
        } else {
            /* initialise the new profile */
            memset((void*)modem->vbc_ctrl_pipe_info, 0x00, modem->num * sizeof(vbc_ctrl_pipe_para_t));
        }
    }

    AUDIO_LOG_DEBUG("peter: modem num is %d",modem->num);
    /* return the profile just added */
    return modem->vbc_ctrl_pipe_info;
}


static void __audio_modem_start_tag(void *data, const XML_Char *tag_name,
        const XML_Char **attr)
{
    struct modem_config_parse_state *state = data;
    audio_modem_t *modem = state->modem_info;

    /* Look at tags */
    if (strcmp(tag_name, "audio") == 0) {
        if (strcmp(attr[0], "device") == 0) {
            AUDIO_LOG_INFO("The device name is %s", attr[1]);
        } else {
            AUDIO_LOG_ERROR("Unnamed audio!");
        }
    } else if (strcmp(tag_name, "modem") == 0) {
        /* Obtain the modem num */
        if (strcmp(attr[0], "num") == 0) {
            AUDIO_LOG_DEBUG("The modem num is '%s'", attr[1]);
            state->vbc_ctrl_pipe_info = __audio_modem_create(modem, attr[1]);
        } else {
            AUDIO_LOG_ERROR("no modem num!");
        }
    } else if (strcmp(tag_name, "cp") == 0) {
        if (state->vbc_ctrl_pipe_info) {
            /* Obtain the modem name  \pipe\vbc   filed */
            if (strcmp(attr[0], "name") != 0) {
                AUDIO_LOG_ERROR("Unnamed modem!");
                goto attr_err;
            }
            if (strcmp(attr[2], "pipe") != 0) {
                AUDIO_LOG_ERROR("'%s' No pipe filed!", attr[0]);
                goto attr_err;
            }
            if (strcmp(attr[4], "vbchannel") != 0) {
                AUDIO_LOG_ERROR("'%s' No vbc filed!", attr[0]);
                goto attr_err;
            }
            AUDIO_LOG_DEBUG("cp name is '%s', pipe is '%s',vbc is '%s'", attr[1], attr[3],attr[5]);
            if(strcmp(attr[1], "w") == 0)
            {
                state->vbc_ctrl_pipe_info->cp_type = CP_W;
            }
            else if(strcmp(attr[1], "t") == 0)
            {
                state->vbc_ctrl_pipe_info->cp_type = CP_TG;
            }
            memcpy((void*)state->vbc_ctrl_pipe_info->s_vbc_ctrl_pipe_name,(void*)attr[3],strlen((char *)attr[3]));
            state->vbc_ctrl_pipe_info->channel_id = atoi((char *)attr[5]);
            state->vbc_ctrl_pipe_info++;

        } else {
            AUDIO_LOG_ERROR("error profile!");
        }
    } else if (strcmp(tag_name, "i2s_for_btcall") == 0) {
        if (strcmp(attr[0], "index") == 0) {
            AUDIO_LOG_DEBUG("The iis_for_btcall index is '%s'", attr[1]);
            modem->i2s_bt.index = atoi((char *)attr[1]);
        } else {
            AUDIO_LOG_ERROR("no iis_ctl index for bt call!");
        }

        if (strcmp(attr[2], "switch") == 0) {
            AUDIO_LOG_DEBUG("The iis_for_btcall switch is '%s'", attr[3]);
            if(strcmp(attr[3],"1") == 0)
                modem->i2s_bt.is_switch = true;
            else if(strcmp(attr[3],"0") == 0)
                modem->i2s_bt.is_switch = false;
        } else {
            AUDIO_LOG_ERROR("no iis_ctl switch for bt call!");
        }
        if (strcmp(attr[4], "dst") == 0) {
            AUDIO_LOG_DEBUG("The iis_for_btcall dst  is '%s'", attr[5]);
            if (strcmp(attr[5], "internal") == 0)
                modem->i2s_bt.is_ext = 0;
            else if (strcmp(attr[5], "external") == 0)
                modem->i2s_bt.is_ext = 1;
        } else {
            AUDIO_LOG_ERROR("no dst path for bt call!");
        }
    } else if (strcmp(tag_name, "i2s_for_extspeaker") == 0) {
        if (strcmp(attr[0], "index") == 0) {
            AUDIO_LOG_DEBUG("The i2s_for_extspeaker index is '%s'", attr[1]);
            modem->i2s_extspk.index = atoi((char *)attr[1]);
        } else {
            AUDIO_LOG_ERROR("no iis_ctl index for extspk call!");
        }
        if (strcmp(attr[2], "switch") == 0) {
            AUDIO_LOG_DEBUG("The iis_for_btcall switch is '%s'", attr[3]);
            if(strcmp(attr[3],"1") == 0)
                modem->i2s_extspk.is_switch = true;
            else if(strcmp(attr[3],"0") == 0)
                modem->i2s_extspk.is_switch = false;
        } else {
            AUDIO_LOG_ERROR("no iis_ctl switch for extspk call!");
        }
        if (strcmp(attr[4], "dst") == 0) {
            if (strcmp(attr[5], "external") == 0)
                modem->i2s_extspk.is_ext = 1;
            else if(strcmp(attr[5], "internal") == 0)
                modem->i2s_extspk.is_ext = 0;

            AUDIO_LOG_DEBUG("The i2s_for_extspeaker dst  is '%d'", modem->i2s_extspk.is_ext);

        } else {
            AUDIO_LOG_ERROR("no dst path for bt call!");
        }
    } else if (strcmp(tag_name, "debug") == 0) {  //parse debug info
        if (strcmp(attr[0], "enable") == 0) {
            if (strcmp(attr[1], "0") == 0) {
                modem->debug_info.enable = 0;
            } else {
                modem->debug_info.enable = 1;
            }
        } else {
            AUDIO_LOG_ERROR("no adaptable type for debug!");
            goto attr_err;
        }
    } else if (strcmp(tag_name, "debuginfo") == 0) { //parse debug info
        if (strcmp(attr[0], "sleepdeltatimegate") == 0) {
            AUDIO_LOG_DEBUG("The sleepdeltatimegate is  '%s'", attr[1]);
            modem->debug_info.sleeptime_gate=atoi((char *)attr[1]);
        } else if (strcmp(attr[0], "pcmwritetimegate") == 0) {
            AUDIO_LOG_DEBUG("The pcmwritetimegate is  '%s'", attr[1]);
            modem->debug_info.pcmwritetime_gate=atoi((char *)attr[1]);
        } else if (strcmp(attr[0], "lastthiswritetimegate") == 0) {
            AUDIO_LOG_DEBUG("The lastthiswritetimegate is  '%s'", attr[1]);
            modem->debug_info.lastthis_outwritetime_gate=atoi((char *)attr[1]);
        } else {
            AUDIO_LOG_ERROR("no adaptable info for debuginfo!");
            goto attr_err;
        }
   }

attr_err:
    return;
}
static void __audio_modem_end_tag(void *data, const XML_Char *tag_name)
{
    return;
}

struct config_parse_state {
    audio_modem_t *modem_info;
 /* To do : pga control setting*/
 /* struct audio_pga *pga; */
 /* struct pga_profile *profile; */
 /* struct pga_attribute_item *attribute_item; */
};
static audio_modem_t * __audio_modem_parse (void)
{
    struct config_parse_state state;
    XML_Parser parser;
    FILE *file;
    int bytes_read;
    void *buf;
    audio_modem_t *modem = NULL;

    modem = calloc(1, sizeof(audio_modem_t));

    if(modem == NULL) {
        goto err_alloc;
    }
    memset(modem, 0, sizeof(audio_modem_t));
    modem->num = 0;
    modem->vbc_ctrl_pipe_info = NULL;

    file = fopen(AUDIO_XML_PATH, "r");
    if (!file) {
        AUDIO_LOG_ERROR("Failed to open %s", AUDIO_XML_PATH);
        goto err_fopen;
    }

    parser = XML_ParserCreate(NULL);
    if (!parser) {
        AUDIO_LOG_ERROR("Failed to create XML parser");
        goto err_parser_create;
    }

    memset(&state, 0, sizeof(state));
    state.modem_info = modem;
    XML_SetUserData(parser, &state);
    XML_SetElementHandler(parser, __audio_modem_start_tag, __audio_modem_end_tag);

    for (;;) {
        buf = XML_GetBuffer(parser, BUF_SIZE);
        if (buf == NULL)
            goto err_parse;

        bytes_read = fread(buf, 1, BUF_SIZE, file);
        if (bytes_read < 0)
            goto err_parse;

        if (XML_ParseBuffer(parser, bytes_read, bytes_read == 0) == XML_STATUS_ERROR) {
            AUDIO_LOG_ERROR("Error in codec PGA xml (%s)", AUDIO_XML_PATH);
            goto err_parse;
        }

        if (bytes_read == 0)
            break;
    }
    XML_ParserFree(parser);
    fclose(file);
    return modem;

err_parse:
    XML_ParserFree(parser);
err_parser_create:
    fclose(file);
err_fopen:
    free(modem);
err_alloc:
    modem = NULL;
    return NULL;
}

audio_return_t _audio_modem_init(audio_hal_t *ah)
{
    audio_return_t audio_ret = AUDIO_RET_OK;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);

    ah->modem.vbc.vbpipe_count = 0;

    /* Initialize vbc interface */
    audio_ret = __vbc_control_open(ah);
    if (AUDIO_IS_ERROR(audio_ret)) {
        AUDIO_LOG_ERROR("__vbc_control_open failed");
        goto exit;
    }
    ah->modem.vbc.voice_pcm_handle_p = NULL;
    ah->modem.vbc.voice_pcm_handle_c = NULL;
    ah->modem.samplerate = 0;
    ah->modem.cp = __audio_modem_parse();
    if (ah->modem.cp == NULL) {
        AUDIO_LOG_ERROR("modem parse failed");
        goto exit;
    }
    ah->modem.cp_type = ah->modem.cp->vbc_ctrl_pipe_info->cp_type;

    /* This ctrl need to be set "0" always - SPRD */
    _audio_mixer_control_set_value(ah, PIN_SWITCH_BT_IIS_CON_SWITCH, 0);

exit:
    return audio_ret;
}

audio_return_t _audio_modem_deinit(audio_hal_t *ah)
{
    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);

    /* Close vbc interface */
    __vbc_control_close(ah);

    return AUDIO_RET_OK;
}

