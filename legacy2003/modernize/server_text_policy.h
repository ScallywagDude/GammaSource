#ifndef HL2MODERN_SERVER_TEXT_POLICY_H
#define HL2MODERN_SERVER_TEXT_POLICY_H
// Portable C++98 server-side mapcycle and MOTD helpers. None of these
// operations read beyond the supplied file length or mutate engine buffers.
#include <stddef.h>
#include <string.h>

namespace HL2ModernServerText {
    enum { MAX_MAPS = 256, MAX_MAP_NAME = 31,
           MAX_MAPCYCLE_BYTES = 131072, MOTD_CHUNK_BYTES = 60,
           MOTD_TOTAL_BYTES = 240 };

    struct MapList {
        char name[MAX_MAPS][MAX_MAP_NAME + 1];
        int count;
        MapList() : count(0) {}
    };

    inline bool MapNameCharacter(unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-';
    }
    inline bool SafeMapName(const char *name) {
        if (!name || !*name) return false;
        unsigned int i = 0;
        while (name[i] != 0) {
            if (i >= MAX_MAP_NAME || !MapNameCharacter((unsigned char)name[i]))
                return false;
            ++i;
        }
        return i > 0;
    }
    inline bool Space(unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
               c == '\v' || c == '\f';
    }
    inline bool LineComment(const char *data, size_t len, size_t i) {
        return i < len && (data[i] == '#' || data[i] == ';' ||
            (data[i] == '/' && i + 1 < len && data[i+1] == '/'));
    }
    inline void SkipComment(const char *data, size_t len, size_t &i) {
        while (i < len && data[i] != '\r' && data[i] != '\n') ++i;
    }

    inline MapList ParseMapcycle(const char *data, size_t length) {
        MapList result;
        if (!data) return result;
        if (length > MAX_MAPCYCLE_BYTES) length = MAX_MAPCYCLE_BYTES;
        size_t i = 0;
        // UTF-8 BOM produced by some modern text editors.
        if (length >= 3 && (unsigned char)data[0] == 0xEF &&
            (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
            i = 3;
        while (i < length && result.count < MAX_MAPS) {
            if (data[i] == 0) break;
            if (Space((unsigned char)data[i])) { ++i; continue; }
            if (LineComment(data, length, i)) {
                SkipComment(data, length, i);
                continue;
            }
            char token[MAX_MAP_NAME + 1];
            size_t n = 0;
            bool safe = true;
            while (i < length && data[i] != 0 && !Space((unsigned char)data[i])) {
                if (n < MAX_MAP_NAME && MapNameCharacter((unsigned char)data[i]))
                    token[n++] = data[i];
                else
                    safe = false;
                ++i;
            }
            if (!safe || n == 0) continue; // Reject, do not truncate.
            token[n] = 0;
            memcpy(result.name[result.count], token, n + 1);
            ++result.count;
        }
        return result;
    }

    typedef bool (*MapValidator)(const char *, void *);
    // Returns false if none of the bounded candidate names is installed.
    inline bool SelectNextMap(const MapList &maps, const char *current,
                              MapValidator valid, void *context,
                              char *out, size_t outSize) {
        if (!out || !outSize) return false;
        out[0] = 0;
        if (!valid || maps.count <= 0) return false;
        int start = 0;
        if (current) {
            for (int i = 0; i < maps.count; ++i) {
                if (strcmp(maps.name[i], current) == 0) {
                    start = (i + 1) % maps.count;
                    break;
                }
            }
        }
        for (int step = 0; step < maps.count; ++step) {
            const char *name = maps.name[(start + step) % maps.count];
            const size_t length = strlen(name);
            if (length + 1 <= outSize && valid(name, context)) {
                memcpy(out, name, length + 1);
                return true;
            }
        }
        return false;
    }

    struct MotdChunk {
        char text[MOTD_CHUNK_BYTES + 1];
        int bytes;
        bool final;
        MotdChunk() : bytes(0), final(true) { text[0] = 0; }
    };
    inline MotdChunk ReadMotdChunk(const char *data, int length, int offset) {
        MotdChunk result;
        if (!data || length <= 0 || offset < 0) return result;
        const int limit = length < MOTD_TOTAL_BYTES ? length : MOTD_TOTAL_BYTES;
        if (offset >= limit) return result;
        int i = 0;
        for (; i < MOTD_CHUNK_BYTES && offset + i < limit; ++i) {
            if (data[offset + i] == 0) break;
            result.text[i] = data[offset + i];
        }
        result.bytes = i;
        result.text[i] = 0;
        result.final = offset + i >= limit || data[offset + i] == 0;
        return result;
    }
}
#endif
