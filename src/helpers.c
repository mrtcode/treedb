
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <jansson.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include "helpers.h"

uint64_t hex2id64(const char *hex) {
    uint64_t id;
    uint8_t *p;
    uint8_t a, b;
    int i;

    if (strlen(hex) != 16)
        return 0;

    for (i = 0, p = (uint8_t *) hex; i < 8; i++) {

        a = *p;
        if (a >= '0' && a <= '9')
            a = a - '0';
        else if (a >= 'a' && a <= 'f')
            a = a - 'a' + 10;
        else if (a >= 'A' && a <= 'F')
            a = a - 'A' + 10;
        else
            return 0;

        b = *(p + 1);
        if (b >= '0' && b <= '9')
            b = b - '0';
        else if (b >= 'a' && b <= 'f')
            b = b - 'a' + 10;
        else if (b >= 'A' && b <= 'F')
            b = b - 'A' + 10;
        else
            return 0;

        ((uint8_t *) &id)[7 - i] = (a << 4) | b;
        p += 2;
    }

    return id;
}

char *id2hex64(char *hex, uint64_t id) {
    static char byteMap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    int i;
    for (i = 0; i < 8; i++) {
        hex[i * 2] = byteMap[((uint8_t *) &id)[7 - i] >> 4];
        hex[i * 2 + 1] = byteMap[((uint8_t *) &id)[7 - i] & 0x0f];
    }
    hex[16] = 0;
    return hex;
}

uint32_t hex2id32(const char *hex) {
    uint32_t id=0;
    uint8_t *p;
    uint8_t a, b;
    int i;

    if (strlen(hex) != 8)
        return 0;

    for (i = 0, p = (uint8_t *) hex; i < 4; i++) {

        a = *p;
        if (a >= '0' && a <= '9')
            a = a - '0';
        else if (a >= 'a' && a <= 'f')
            a = a - 'a' + 10;
        else if (a >= 'A' && a <= 'F')
            a = a - 'A' + 10;
        else
            return 0;

        b = *(p + 1);
        if (b >= '0' && b <= '9')
            b = b - '0';
        else if (b >= 'a' && b <= 'f')
            b = b - 'a' + 10;
        else if (b >= 'A' && b <= 'F')
            b = b - 'A' + 10;
        else
            return 0;

        ((uint8_t *) &id)[3 - i] = (a << 4) | b;
        p += 2;
    }

    return id;
}

char *id2hex32(char *hex, uint32_t id) {
    static char byteMap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    int i;
    for (i = 0; i < 4; i++) {
        hex[i * 2] = byteMap[((uint8_t *) &id)[3 - i] >> 4];
        hex[i * 2 + 1] = byteMap[((uint8_t *) &id)[3 - i] & 0x0f];
    }
    hex[8] = 0;
    return hex;
}

//Convert a buffer of binary values into a hex string representation
char *bufToHex(uint8_t *bytes, size_t buflen) {
    static char byteMap[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    char *retval;
    int i;

    retval = malloc(buflen * 2 + 1);
    for (i = 0; i < buflen; i++) {
        retval[i * 2] = byteMap[bytes[i] >> 4];
        retval[i * 2 + 1] = byteMap[bytes[i] & 0x0f];
    }
    retval[i * 2 + 1] = '\0';
    return retval;
}

size_t utf8len(char *s) {
    size_t len = 0;
    for (; *s; ++s) if ((*s & 0xC0) != 0x80) ++len;
    return len;
}

char *utf8index(char *s, size_t pos) {
    ++pos;
    for (; *s; ++s) {
        if ((*s & 0xC0) != 0x80) --pos;
        if (pos == 0) return s;
    }
    return NULL;
}

void utf8slice2(char *s, ssize_t *start, ssize_t *end) {
    char *p = utf8index(s, *start);
    *start = p ? p - s : -1;
    p = utf8index(s, *end);
    *end = p ? p - s : -1;
}

char *utf8slice(char *utf8str, size_t max) {
    char *s = utf8str;
    size_t utf8len = 0;
    while (*s) {
        if ((*s & 0xC0) != 0x80) {
            if (utf8len >= max) break;
            utf8len++;
        }

        s++;
    }

    size_t len = s - utf8str;

    if (len > max * 4) len = max * 4;

    char *str = malloc(len + 1);

    memcpy(str, utf8str, len);

    str[len] = 0;

    return str;
}

int json_get_id32(json_t *parent, char *name, uint32_t *id) {
    const char *hexid;
    json_t *obj;
    obj = json_object_get(parent, name);

    if (json_is_integer(obj) && json_integer_value(obj) == 0) {
        *id = 0;
        return 1;
    }

    if ((hexid = json_string_value(obj))) {
        *id = hex2id32(hexid);
        return 1;
    }

    return 0;
}

int json_get_id64(json_t *parent, char *name, uint64_t *id) {
    const char *hexid;
    json_t *obj;
    obj = json_object_get(parent, name);

    if (json_is_integer(obj) && json_integer_value(obj) == 0) {
        *id = 0;
        return 1;
    }

    if ((hexid = json_string_value(obj))) {
        *id = hex2id64(hexid);
        return 1;
    }

    return 0;
}

int json_get_id64s(json_t *parent, char *name, uint64_t **ids, uint32_t *ids_n) {

    *ids_n = 0;

    json_t *obj;
    obj = json_object_get(parent, name);


    if (json_is_array(obj)) {

        uint32_t n = (uint32_t)json_array_size(obj);

        if (n > 10000) n = 10000;

        *ids = malloc(n * sizeof(uint64_t));
        *ids_n = n;

        int i;
        for (i = 0; i < n; i++) {
            json_t *el = json_array_get(obj, (size_t)i);
            if (json_is_string(el)) {
                (*ids)[i] = hex2id64(json_string_value(el));
            }
        }
    }
    return 1;
}

int json_get_str(json_t *parent, char *name, char **str) {
    json_t *obj;
    obj = json_object_get(parent, name);
    if (!(*str = json_string_value(obj))) {
        return 0;
    }
    return 1;
}

int json_get_strs(json_t *parent, char *name, char ***strs, uint32_t *strs_n) {

    *strs_n = 0;

    json_t *obj;
    obj = json_object_get(parent, name);


    if (json_is_array(obj)) {

        uint32_t n = (uint32_t) json_array_size(obj);

        if (n > 10000) n = 10000;

        *strs = malloc(n * sizeof(char *));
        *strs_n = n;

        int i;
        for (i = 0; i < n; i++) {
            json_t *el = json_array_get(obj, i);
            if (json_is_string(el)) {
                char *a = json_string_value(el);
                (*strs)[i] = a;
            }
        }
    }
    return 1;
}

int json_get_int16(json_t *parent, char *name, uint16_t *integer) {
    json_t *obj;
    obj = json_object_get(parent, name);
    if (json_is_integer(obj)) {
        *integer = (uint16_t) json_integer_value(obj);
        return 1;
    }
    return 0;
}

int copyfile(char *from, char *to) {

    unlink(to);

    int read_fd;
    int write_fd;
    struct stat stat_buf;
    off_t offset = 0;

    read_fd = open(from, O_RDONLY);
    fstat(read_fd, &stat_buf);
    write_fd = open(to, O_WRONLY | O_CREAT, stat_buf.st_mode);
    sendfile(write_fd, read_fd, &offset, stat_buf.st_size); //todo: check if file is really copied
    close(read_fd);
    close(write_fd);

    return 1;
}
