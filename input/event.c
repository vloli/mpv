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
        // Forward dropped files to clients so they can be handled by user
        // scripts via the "mpv_internal_drop_files_forward" script-message.
        if (num_files > 0) {
            int cmd_size = 2 + num_files + 1;
            const char **cmd = talloc_array(NULL, const char *, cmd_size);
            if (cmd) {
                cmd[0] = "script-message";
                cmd[1] = "mpv_internal_drop_files_forward";
                for (int i = 0; i < num_files; i++)
                    cmd[2 + i] = files[i];
                cmd[cmd_size - 1] = NULL;

                mp_input_run_cmd(ictx, cmd);
                talloc_free(cmd);
                talloc_free(tmp);
                return 1;
            }
        }

        mp_input_drop_files(ictx, num_files, files, action);
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
