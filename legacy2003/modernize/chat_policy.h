#ifndef HL2MODERN_CHAT_POLICY_H
#define HL2MODERN_CHAT_POLICY_H

// Source 2003 modernization: self-contained and testable multiplayer chat policy.
// This header does not depend on the Steamworks SDK or the original engine.
// Drop C0 controls and DEL from user chat, bound output, and throttle per connection.
namespace HL2ModernChat
{
    struct Slot
    {
        int userid;
        float next_allowed;
        bool valid;
        Slot() : userid(-1), next_allowed(0.0f), valid(false) {}
    };

    inline void Reset(Slot &slot)
    {
        slot.userid = -1;
        slot.next_allowed = 0.0f;
        slot.valid = false;
    }

    // The user ID is supplied by the engine; it changes when a slot is reused.
    // Time moving backward on level transitions resets the throttle as well.
    inline bool Admit(Slot &slot, int userid, float now, float cooldown)
    {
        if ( !slot.valid || slot.userid != userid || now + 30.0f < slot.next_allowed )
        {
            slot.userid = userid;
            slot.next_allowed = 0.0f;
            slot.valid = true;
        }
        if (cooldown <= 0.0f)
        {
            slot.next_allowed = now;
            return true; // operators can disable throttling immediately
        }
        if (cooldown > 30.0f) cooldown = 30.0f;
        if (now < slot.next_allowed) return false;
        slot.next_allowed = now + cooldown;
        return true;
    }

    // Removes wrapping quotes, CR/LF and control bytes without changing input.
    // Returns character count. Overlapping buffers are not supported.
    inline unsigned int Clean(const char *source, char *dest, unsigned int dest_size)
    {
        if (!dest || dest_size == 0) return 0;
        dest[0] = 0;
        if (!source) return 0;
        const char *begin = source;
        unsigned int length = 0;
        while (source[length]) ++length;
        if (length >= 2 && source[0] == '"' && source[length - 1] == '"')
        {
            ++begin;
            length -= 2;
        }
        unsigned int written = 0;
        for (unsigned int i = 0; i < length && written + 1 < dest_size; ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(begin[i]);
            if (ch == '\r' || ch == '\n' || ch == '\t')
            {
                if (written && dest[written-1] != ' ') dest[written++] = ' ';
            }
            else if (ch >= 32 && ch != 127)
                dest[written++] = static_cast<char>(ch);
        }
        while (written && dest[written - 1] == ' ') --written;
        dest[written] = 0;
        return written;
    }

    inline bool HasVisibleText(const char *text)
    {
        if (!text) return false;
        for (const unsigned char *s = reinterpret_cast<const unsigned char *>(text); *s; ++s)
            if (*s != ' ' && *s != '\t') return true;
        return false;
    }
}

#endif
