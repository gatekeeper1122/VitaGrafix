#include <vitasdk.h>
#include <taihen.h>

#include "io.h"
#include "log.h"
#include "config.h"
#include "patch.h"
#include "patch_gens.h"
#include "patch_tools.h"
#include "main.h"

int vgPatchGetNextMacroArgPos(const char chunk[], int pos, int end) {
    int inner_open = 0;

    while (pos < end - 3 && (inner_open > 0 || chunk[pos] != ',')) {
        // Allow stacking, e.g.: </,<*,<ib_w>,10>,10>
        if (chunk[pos] == '<')
            inner_open++;
        if (chunk[pos] == '>')
            inner_open--;
        pos++;
    }

    return pos + 1;
}

VG_IoParseState vgPatchParseGenValue(
        const char chunk[], int pos, int end,
        uint32_t *value) {

    // Check for macros
    if (chunk[pos] == '<') {
        if (!strncasecmp(&chunk[pos], "<fb_w>", 6)) {
            *value = g_main.config.fb.width;
            return IO_OK;
        }
        if (!strncasecmp(&chunk[pos], "<fb_h>", 6)) {
            *value = g_main.config.fb.height;
            return IO_OK;
        }
        if (!strncasecmp(&chunk[pos], "<ib_w", 5)) {
            if (chunk[pos + 5] == '>') {
                *value = g_main.config.ib[0].width;
                return IO_OK;
            }
            else if (chunk[pos + 5] == ',') {
                uint8_t ib_n = strtoul(&chunk[pos + 6], NULL, 10);
                if (ib_n >= MAX_RES_COUNT) {
                    vgLogPrintF("[PATCH] ERROR: Accessed [%u] IB res out of range!\n", ib_n);
                    return IO_BAD;
                }

                vgConfigSetSupportedIbCount(ib_n + 1); // Raise supp. IB count
                *value = g_main.config.ib[ib_n].width;
                return IO_OK;
            }
        }
        if (!strncasecmp(&chunk[pos], "<ib_h", 5)) {
            if (chunk[pos + 5] == '>') {
                *value = g_main.config.ib[0].height;
                return IO_OK;
            }
            else if (chunk[pos + 5] == ',') {
                uint8_t ib_n = strtoul(&chunk[pos + 6], NULL, 10);
                if (ib_n >= MAX_RES_COUNT) {
                    vgLogPrintF("[PATCH] ERROR: Accessed [%u] IB res out of range!\n", ib_n);
                    return IO_BAD;
                }

                vgConfigSetSupportedIbCount(ib_n + 1); // Raise supp. IB count
                *value = g_main.config.ib[ib_n].height;
                return IO_OK;
            }
        }
        if (!strncasecmp(&chunk[pos], "<vblank>", 8)) {
            *value = g_main.config.fps == FPS_60 ? 1 : 2;
            return IO_OK;
        }
        if (!strncasecmp(&chunk[pos], "<msaa>", 6)) {
            *value = g_main.config.msaa == MSAA_4X ? 2 :
                    (g_main.config.msaa == MSAA_2X ? 1 : 0);
            return IO_OK;
        }
        if (!strncasecmp(&chunk[pos], "<msaa_enabled>", 14)) {
            *value = g_main.config.msaa_enabled == FT_ENABLED;
            return IO_OK;
        }
        if (!strncasecmp(&chunk[pos], "<+,", 3) ||
                !strncasecmp(&chunk[pos], "<-,", 3) ||
                !strncasecmp(&chunk[pos], "<*,", 3) ||
                !strncasecmp(&chunk[pos], "</,", 3) ||
                !strncasecmp(&chunk[pos], "<&,", 3) ||
                !strncasecmp(&chunk[pos], "<|,", 3) ||
                !strncasecmp(&chunk[pos], "<l,", 3) ||
                !strncasecmp(&chunk[pos], "<r,", 3) ||
                !strncasecmp(&chunk[pos], "<min,", 5) ||
                !strncasecmp(&chunk[pos], "<max,", 5)) {
            int token_pos = pos + 3;
            if (tolower(chunk[pos + 1]) == 'm') {
                token_pos += 2;
            }
            uint32_t a, b;

            if (vgPatchParseGenValue(chunk, token_pos, end, &a))
                return IO_BAD;

            token_pos = vgPatchGetNextMacroArgPos(chunk, token_pos, end);

            if (vgPatchParseGenValue(chunk, token_pos, end, &b))
                return IO_BAD;

            if (chunk[pos + 1] == '+')
                *value = a + b;
            else if (chunk[pos + 1] == '-')
                *value = a - b;
            else if (chunk[pos + 1] == '*')
                *value = a * b;
            else if (chunk[pos + 1] == '/')
                *value = a / b;
            else if (chunk[pos + 1] == '&')
                *value = a & b;
            else if (chunk[pos + 1] == '|')
                *value = a | b;
            else if (tolower(chunk[pos + 1]) == 'l')
                *value = a << b;
            else if (tolower(chunk[pos + 1]) == 'r')
                *value = a >> b;
            else if (tolower(chunk[pos + 1]) == 'm' && tolower(chunk[pos + 3]) == 'n')
                *value = a < b ? a : b;
            else if (tolower(chunk[pos + 1]) == 'm' && tolower(chunk[pos + 3]) == 'x')
                *value = a > b ? a : b;
            return IO_OK;
        }
        if (!strncasecmp(&chunk[pos], "<to_fl,", 7)) {
            uint32_t a;

            if (vgPatchParseGenValue(chunk, pos + 7, end, &a))
                return IO_BAD;

            float a_fl = (float)a;
            memcpy(value, &a_fl, sizeof(uint32_t));
            return IO_OK;
        }
        if (!strncasecmp(&chunk[pos], "<if_eq,", 7) ||
                !strncasecmp(&chunk[pos], "<if_gt,", 7) ||
                !strncasecmp(&chunk[pos], "<if_lt,", 7) ||
                !strncasecmp(&chunk[pos], "<if_ge,", 7) ||
                !strncasecmp(&chunk[pos], "<if_le,", 7)) {
            int token_pos = pos + 7;
            uint32_t a, b, c, d;

            if (vgPatchParseGenValue(chunk, token_pos, end, &a))
                return IO_BAD;

            token_pos = vgPatchGetNextMacroArgPos(chunk, token_pos, end);
            if (vgPatchParseGenValue(chunk, token_pos, end, &b))
                return IO_BAD;

            token_pos = vgPatchGetNextMacroArgPos(chunk, token_pos, end);
            if (vgPatchParseGenValue(chunk, token_pos, end, &c))
                return IO_BAD;

            token_pos = vgPatchGetNextMacroArgPos(chunk, token_pos, end);
            if (vgPatchParseGenValue(chunk, token_pos, end, &d))
                return IO_BAD;

            if (tolower(chunk[pos + 4]) == 'e') {
                *value = a == b ? c : d;
            } else if (tolower(chunk[pos + 4]) == 'g') {
                if (tolower(chunk[pos + 5]) == 't') {
                    *value = a > b ? c : d;
                } else {
                    *value = a >= b ? c : d;
                }
            } else if (tolower(chunk[pos + 4]) == 'l') {
                if (tolower(chunk[pos + 5]) == 't') {
                    *value = a < b ? c : d;
                } else {
                    *value = a <= b ? c : d;
                }
            }
            return IO_OK;
        }

        vgLogPrintF("[PATCH] ERROR: Invalid macro!\n");
        return IO_BAD; // Invalid macro
    }

    // Regular value
    *value = strtoul(&chunk[pos], NULL, 0);
    return IO_OK;
}

VG_IoParseState vgPatchParseGen(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!vgPatchParseGen_uint16(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_uint32(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_fl32(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_bytes(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_nop(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_bkpt(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_a1_mov(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_a2_mov(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_t1_mov(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_t2_mov(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_t3_mov(chunk, pos, end, patch_data, patch_data_len) ||
            !vgPatchParseGen_t1_movt(chunk, pos, end, patch_data, patch_data_len)) {
        return IO_OK;
    }

    vgLogPrintF("[PATCH] ERROR: Invalid generator!\n");
    return IO_BAD;
}

/**
 * uint16(255)    -> FF 00
 * uint16(321)    -> 41 01
 * uint16(0x4422) -> 22 44
 */
VG_IoParseState vgPatchParseGen_uint16(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "uint16(", 7)) {
        int token_end = pos;
        uint32_t value = 0;
        while (chunk[token_end] != ')') { token_end++; }

        if (vgPatchParseGenValue(chunk, pos + 7, token_end, &value))
            return IO_BAD;

        *patch_data_len = 2;
        patch_data[0] = value & 0xFF;
        patch_data[1] = (value >> 8) & 0xFF;
        return IO_OK;
    }

    return IO_BAD;
}

/**
 * uint32(960)        -> C0 03 00 00
 * uint32(0x81234567) -> 67 45 23 81
 */
VG_IoParseState vgPatchParseGen_uint32(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "uint32(", 7)) {
        int token_end = pos;
        uint32_t value = 0;
        while (chunk[token_end] != ')') { token_end++; }

        if (vgPatchParseGenValue(chunk, pos + 7, token_end, &value))
            return IO_BAD;

        *patch_data_len = 4;
        patch_data[0] = value & 0xFF;
        patch_data[1] = (value >> 8) & 0xFF;
        patch_data[2] = (value >> 16) & 0xFF;
        patch_data[3] = (value >> 24) & 0xFF;
        return IO_OK;
    }

    return IO_BAD;
}

/**
 * fl32(960) -> 00 00 70 44
 * fl32(840) -> 00 00 52 44 
 */
VG_IoParseState vgPatchParseGen_fl32(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "fl32(", 5)) {
        int token_end = pos;
        uint32_t value = 0;
        while (chunk[token_end] != ')') { token_end++; }

        if (vgPatchParseGenValue(chunk, pos + 5, token_end, &value))
            return IO_BAD;

        *patch_data_len = 4;
        float flvalue = (float)value;
        memcpy(&value, &flvalue, sizeof(uint32_t));
        patch_data[0] = value & 0xFF;
        patch_data[1] = (value >> 8) & 0xFF;
        patch_data[2] = (value >> 16) & 0xFF;
        patch_data[3] = (value >> 24) & 0xFF;
        return IO_OK;
    }

    return IO_BAD;
}

/**
 * bytes(DEAD)        -> DE AD
 * bytes(01020304)    -> 01 02 03 04
 * bytes(BE EF DE AD) -> BE AF DE AD
 */
VG_IoParseState vgPatchParseGen_bytes(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "bytes(", 6)) {
        int token_end = pos;
        uint32_t value;

        *patch_data_len = 0;
        token_end += 6;

        while (*patch_data_len < PATCH_MAX_LENGTH && token_end < end && chunk[token_end] != ')') {
            while (isspace(chunk[token_end])) { token_end++; }

            char byte[3] = ""; // take only one byte at a time
            memcpy(&byte, &chunk[token_end], 2);
            value = strtoul(byte, NULL, 16);
            token_end += 2;

            patch_data[*patch_data_len] = value & 0xFF;
            (*patch_data_len)++;

            while (isspace(chunk[token_end])) { token_end++; }
        }

        if (*patch_data_len == PATCH_MAX_LENGTH && chunk[token_end] != ')') {
            vgLogPrintF("[PATCH] ERROR: Patch too long!\n");
            return IO_BAD;
        }

        return IO_OK;
    }

    return IO_BAD;
}

/**
 * nop() -> 00 BF
 */
VG_IoParseState vgPatchParseGen_nop(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "nop(", 4)) {
        *patch_data_len = 2;
        patch_data[0] = 0x00;
        patch_data[1] = 0xBF;
        return IO_OK;
    }

    return IO_BAD;
}

/**
 * bkpt() -> 00 BE
 */
VG_IoParseState vgPatchParseGen_bkpt(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "bkpt(", 5)) {
        *patch_data_len = 2;
        patch_data[0] = 0x00;
        patch_data[1] = 0xBE;
        return IO_OK;
    }

    return IO_BAD;
}

/**
 * a1_mov()
 */
VG_IoParseState vgPatchParseGen_a1_mov(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "a1_mov(", 7)) {
        int token_end = pos;
        uint32_t value;

        *patch_data_len = 4;
        uint32_t setflags = 0;
        uint32_t reg = 0;

        while (chunk[token_end] != ',') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos + 7, token_end, &setflags))
            return IO_BAD;

        token_end++;
        pos = token_end;
        while (chunk[token_end] != ',') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos, token_end, &reg))
            return IO_BAD;

        token_end++;
        pos = token_end;
        while (chunk[token_end] != ')') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos, token_end, &value))
            return IO_BAD;

        vgMakeArm_A1_MOV(reg, setflags, value, patch_data);
        return IO_OK;
    }

    return IO_BAD;
}

/**
 * a2_mov()
 */
VG_IoParseState vgPatchParseGen_a2_mov(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "a2_mov(", 7)) {
        int token_end = pos;
        uint32_t value;

        *patch_data_len = 4;
        uint32_t reg = 0;

        while (chunk[token_end] != ',') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos + 7, token_end, &reg))
            return IO_BAD;

        token_end++;
        pos = token_end;
        while (chunk[token_end] != ')') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos, token_end, &value))
            return IO_BAD;

        vgMakeArm_A2_MOV(reg, value, patch_data);
        return IO_OK;
    }

    return IO_BAD;
}

VG_IoParseState vgPatchParseGen_t1_mov(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "t1_mov(", 7)) {
        int token_end = pos;
        uint32_t value;

        *patch_data_len = 2;
        uint32_t reg = 0;

        while (chunk[token_end] != ',') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos + 7, token_end, &reg))
            return IO_BAD;

        token_end++;
        pos = token_end;
        while (chunk[token_end] != ')') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos, token_end, &value))
            return IO_BAD;

        vgMakeThumb_T1_MOV(reg, value, patch_data);
        return IO_OK;
    }

    return IO_BAD;
}

VG_IoParseState vgPatchParseGen_t2_mov(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "t2_mov(", 7)) {
        int token_end = pos;
        uint32_t value;

        *patch_data_len = 4;
        uint32_t setflags = 0;
        uint32_t reg = 0;

        while (chunk[token_end] != ',') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos + 7, token_end, &setflags))
            return IO_BAD;

        token_end++;
        pos = token_end;
        while (chunk[token_end] != ',') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos, token_end, &reg))
            return IO_BAD;

        token_end++;
        pos = token_end;
        while (chunk[token_end] != ')') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos, token_end, &value))
            return IO_BAD;

        vgMakeThumb2_T2_MOV(reg, setflags, value, patch_data);
        return IO_OK;
    }

    return IO_BAD;
}

VG_IoParseState vgPatchParseGen_t3_mov(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "t3_mov(", 7)) {
        int token_end = pos;
        uint32_t value;

        *patch_data_len = 4;
        uint32_t reg = 0;

        while (chunk[token_end] != ',') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos + 7, token_end, &reg))
            return IO_BAD;

        token_end++;
        pos = token_end;
        while (chunk[token_end] != ')') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos, token_end, &value))
            return IO_BAD;

        vgMakeThumb2_T3_MOV(reg, value, patch_data);
        return IO_OK;
    }

    return IO_BAD;
}

VG_IoParseState vgPatchParseGen_t1_movt(
        const char chunk[], int pos, int end,
        uint8_t patch_data[], uint8_t *patch_data_len) {

    if (!strncasecmp(&chunk[pos], "t1_movt(", 8)) {
        int token_end = pos;
        uint32_t value;

        *patch_data_len = 4;
        uint32_t reg = 0;

        while (chunk[token_end] != ',') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos + 8, token_end, &reg))
            return IO_BAD;

        token_end++;
        pos = token_end;
        while (chunk[token_end] != ')') { token_end++; }
        if (vgPatchParseGenValue(chunk, pos, token_end, &value))
            return IO_BAD;

        vgMakeThumb2_T1_MOVT(reg, value, patch_data);
        return IO_OK;
    }

    return IO_BAD;
}
