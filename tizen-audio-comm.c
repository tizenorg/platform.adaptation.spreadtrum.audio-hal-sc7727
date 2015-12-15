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

#include "tizen-audio-internal.h"

audio_return_t _audio_comm_send_message(audio_hal_t *ah, const char *name, int value)
{
    audio_return_t audio_ret = AUDIO_RET_OK;

    AUDIO_LOG_DEBUG("send message : name(%s), value(%d)", name, value);
    if (ah->comm.msg_cb) {
        ah->comm.msg_cb(name, value, ah->comm.user_data);
    }

    return audio_ret;
}

audio_return_t _audio_comm_init(audio_hal_t *ah)
{
    audio_return_t audio_ret = AUDIO_RET_OK;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);

    return audio_ret;
}

audio_return_t _audio_comm_deinit(audio_hal_t *ah)
{
    audio_return_t audio_ret = AUDIO_RET_OK;

    AUDIO_RETURN_VAL_IF_FAIL(ah, AUDIO_ERR_PARAMETER);

    return audio_ret;
}
