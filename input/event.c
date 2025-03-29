/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "event.h"
#include "input.h"
#include "common/msg.h"
#include "player/external_files.h"
#include "misc/bstr.h" 

void mp_event_drop_files(struct input_ctx *ictx, int num_files, char **files,
                         enum mp_dnd_action action)
{
        if (num_files > 0) {
         // 动态分配 cmd 数组：3个固定参数 + num_files 个文件路径 + 1个 NULL
        int cmd_size = 3 + num_files + 1;
        const char **cmd = talloc_array(NULL, const char *, cmd_size);

        // 填充固定部分
        cmd[0] = "script-message-to";
        cmd[1] = "1mpv";  // 固定前端名称，可调整
        cmd[2] = "load-files";

        // 填充文件路径
        for (int i = 0; i < num_files; i++) {
            cmd[3 + i] = files[i];
        }

        // 末尾置 NULL
        cmd[cmd_size - 1] = NULL;

        // 发送命令
        mp_input_run_cmd(ictx, cmd);

        // 释放内存
        talloc_free(cmd);
        return;
    }
    
    bool all_sub = true;
    for (int i = 0; i < num_files; i++)
        all_sub &= mp_might_be_subtitle_file(files[i]);

    if (all_sub) {
        for (int i = 0; i < num_files; i++) {
            const char *cmd[] = {
                "osd-auto",
                "sub-add",
                files[i],
                NULL
            };
            mp_input_run_cmd(ictx, cmd);
        }
    } else if (action == DND_INSERT_NEXT) {
        /* To insert the entries in the correct order, we iterate over them
           backwards */
        for (int i = num_files - 1; i >= 0; i--) {
            const char *cmd[] = {
                "osd-auto",
                "loadfile",
                files[i],
                /* Since we're inserting in reverse, wait til the final item
                   is added to start playing */
                (i > 0) ? "insert-next" : "insert-next-play",
                NULL
            };
            mp_input_run_cmd(ictx, cmd);
        }
    } else {
        for (int i = 0; i < num_files; i++) {
            const char *cmd[] = {
                "osd-auto",
                "loadfile",
                files[i],
                /* Either start playing the dropped files right away
                   or add them to the end of the current playlist */
                (i == 0 && action == DND_REPLACE) ? "replace" : "append-play",
                NULL
            };
            mp_input_run_cmd(ictx, cmd);
        }
    }
}

int mp_event_drop_mime_data(struct input_ctx *ictx, const char *mime_type,
                            bstr data, enum mp_dnd_action action)
{
    // (text lists are the only format supported right now)
    if (mp_event_get_mime_type_score(ictx, mime_type) >= 0) {
        void *tmp = talloc_new(NULL);
        int num_files = 0;
        char **files = NULL;
        while (data.len) {
            bstr line = bstr_getline(data, &data);
            line = bstr_strip_linebreaks(line);
            if (bstr_startswith0(line, "#") || !line.start[0])
                continue;
            char *s = bstrto0(tmp, line);
            MP_TARRAY_APPEND(tmp, files, num_files, s);
        }
        mp_event_drop_files(ictx, num_files, files, action);
        talloc_free(tmp);
        return num_files > 0;
    } else {
        return -1;
    }
}

int mp_event_get_mime_type_score(struct input_ctx *ictx, const char *mime_type)
{
    // X11 and Wayland file list format.
    if (strcmp(mime_type, "text/uri-list") == 0)
        return 10;
    // Just text; treat it the same for convenience.
    if (strcmp(mime_type, "text/plain;charset=utf-8") == 0)
        return 5;
    if (strcmp(mime_type, "text/plain") == 0)
        return 4;
    if (strcmp(mime_type, "text") == 0)
        return 0;
    return -1;
}
