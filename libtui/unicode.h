#ifndef LIBTUI_UNICODE_INCLUDE
#define LIBTUI_UNICODE_INCLUDE

#define PSH_CORE_NO_PREFIX
#include "psh_core/psh_core.h"

//TODO: make more memory efficient by encoding
//      raw_len and d_width in 1 byte
typedef struct {
    byte raw[4];
    u8 raw_len;
    u8 display_width;
} CodePoint;

list_def(CodePoint);

#define cp(s)      cp_from_utf8(s, sizeof(s) - 1)
CodePoint cp_from_utf8(byte *s, u8 len);
CodePoint cp_from_raw(byte *raw, u8 raw_len, u8 display_width);
CodePoint cp_from_byte(byte b);
b32 cp_equal(CodePoint a, CodePoint b);

typedef u32 Unicode;
u8 unicode_width(Unicode ch);

CodePoint utf8_next(byte **start, byte *end);

static CodePoint UTF8_REPLACEMENT = {
    .raw = {0xEF, 0xBF, 0xBD},
    .raw_len = 3,
    .display_width = 1,
};

#endif

#ifdef LIBTUI_UNICODE_IMPL

CodePoint utf8_next(byte **p, byte *end) {
    byte *s = *p;

    if (s >= end) {
        return UTF8_REPLACEMENT;
    }

    u8 first = s[0];
    if (first < 0x80) {
        *p += 1;
        return cp_from_byte(first);
    }

    usize len = 0;
    Unicode value = 0;

    if ((first & 0xE0) == 0xC0) {
        len = 2;
        value = first & 0x1F;
    } else if ((first & 0xF0) == 0xE0) {
        len = 3;
        value = first & 0x0F;
    } else if ((first & 0xF8) == 0xF0) {
        len = 4;
        value = first & 0x07;
    } else {
        *p += 1;
        return UTF8_REPLACEMENT;
    }

    if (s + len > end) {
        *p = end; // consume rest
        return UTF8_REPLACEMENT;
    }

    for (usize i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *p += i;
            return UTF8_REPLACEMENT;
        }
        value = (value << 6) | (s[i] & 0x3F);
    }

    CodePoint cp = cp_from_raw(s, len, unicode_width(value));
    *p += len;
    return cp;
}

u8 unicode_width(Unicode ch) {
    if (ch < 32 || (ch >= 0x7F && ch < 0xA0))
        return 0;

    // combining marks
    if ((ch >= 0x0300 && ch <= 0x036F) ||
        (ch >= 0x1AB0 && ch <= 0x1AFF) ||
        (ch >= 0x1DC0 && ch <= 0x1DFF) ||
        (ch >= 0x20D0 && ch <= 0x20FF) ||
        (ch >= 0xFE20 && ch <= 0xFE2F))
        return 0;

    // wide characters (CJK + emoji)
    if ((ch >= 0x1100 && ch <= 0x115F) ||
        (ch >= 0x2329 && ch <= 0x232A) ||
        (ch >= 0x2E80 && ch <= 0xA4CF) ||
        (ch >= 0xAC00 && ch <= 0xD7A3) ||
        (ch >= 0xF900 && ch <= 0xFAFF) ||
        (ch >= 0xFE10 && ch <= 0xFE19) ||
        (ch >= 0xFE30 && ch <= 0xFE6F) ||
        (ch >= 0xFF00 && ch <= 0xFF60) ||
        (ch >= 0xFFE0 && ch <= 0xFFE6) ||
        (ch >= 0x1F300 && ch <= 0x1F64F) ||
        (ch >= 0x1F900 && ch <= 0x1F9FF))
        return 2;

    return 1;
}

CodePoint cp_from_utf8(byte *s, u8 len) {
    return utf8_next(&s, s + len);
}

CodePoint cp_from_raw(byte *raw, u8 raw_len, u8 display_width) {
    CodePoint cp = {
        .raw_len = raw_len,
        .display_width = display_width,
    };

    memcpy(cp.raw, raw, raw_len);
    return cp;
}

CodePoint cp_from_byte(byte b) {
    return (CodePoint) {
        .raw = {b},
        .raw_len = 1,
        .display_width = 1,
    };
}

b32 cp_equal(CodePoint a, CodePoint b) {
    if (a.raw_len != b.raw_len) return false;
    if (a.display_width != b.display_width) return false;
    return memcmp(a.raw, b.raw, a.raw_len) == 0;
}

#endif