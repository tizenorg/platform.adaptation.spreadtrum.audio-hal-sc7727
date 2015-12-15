/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VBC_CONTROL_PARAMETERS_H
#define VBC_CONTROL_PARAMETERS_H

#include "pthread.h"


#define BUF_SIZE 1024

#define VBC_PIPE_NAME_MAX_LEN 16
#define VOIP_PIPE_NAME_MAX     VBC_PIPE_NAME_MAX_LEN
#define NAME_LEN_MAX 16

#define AUDIO_XML_PATH "/usr/etc/audio_hw.xml"

#define MODEM_T_ENABLE_PROPERTY     "persist.modem.t.enable"
#define MODEM_W_ENABLE_PROPERTY     "persist.modem.w.enable"


typedef enum {
    CP_W,
    CP_TG,
    CP_MAX
}cp_type_t;

/*support multiple call for multiple modem(cp0/cp1/...):
different modem is corresponding to different pipe and all pipes use the only vbc.
support multiple pipe:
1. change VBC_PIPE_COUNT
2. change the definition of s_vbc_ctrl_pipe_info.
3. change channel_id for different cp .On sharp, 0 for cp0,  1 for cp1,2 for ap
*/

typedef struct
{
    char s_vbc_ctrl_pipe_name[VBC_PIPE_NAME_MAX_LEN];
    int channel_id;
    cp_type_t cp_type;
}vbc_ctrl_pipe_para_t;


struct voip_res
{
    cp_type_t cp_type;
    int8_t  pipe_name[VOIP_PIPE_NAME_MAX];
    int  channel_id;
    int enable;
    int is_done;
    void *adev;
    pthread_t thread_id;
};

typedef struct
{
    int8_t index;
    int is_switch;
    int8_t is_ext;
}i2s_ctl_t;



typedef struct debuginfo
{
    int enable;
    int sleeptime_gate;
    int pcmwritetime_gate;
    int lastthis_outwritetime_gate;
}debuginfo;

typedef struct{
    int num;
    vbc_ctrl_pipe_para_t *vbc_ctrl_pipe_info;
    i2s_ctl_t i2s_bt;
    i2s_ctl_t i2s_extspk;
    struct voip_res  voip_res;
    debuginfo debug_info;
}audio_modem_t;

/*audio mode structure,we can expand  for more fields if necessary*/
typedef struct
{
	int index;
    char mode_name[NAME_LEN_MAX];

}audio_mode_item_t;

/*we mostly have four mode,(headset,headfree,handset,handsfree),
    differet product may configure different mode number,htc have 25 modes.*/
typedef struct{
	int num;
	audio_mode_item_t *audio_mode_item_info;
}aud_mode_t;
struct modem_config_parse_state{
	audio_modem_t *modem_info;
	vbc_ctrl_pipe_para_t *vbc_ctrl_pipe_info;
	aud_mode_t  *audio_mode_info;
	audio_mode_item_t *audio_mode_item_info;
};

#endif

