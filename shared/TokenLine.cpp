#include "TokenLine.h"
#include <ctype.h>
#include <string.h>

TokenLine::TokenLine() { SetLine(""); }
TokenLine::TokenLine(const char *line) { SetLine(line); }

void TokenLine::SetLine(const char *line)
{
    m_tokens.clear();
    m_offsets.clear();
    if (!line) line = "";
    // Tokenize at most one input line. Caller retains ownership of input.
    const char *end = line;
    while (*end && *end != '\r' && *end != '\n') ++end;
    m_line.assign(line, end);
    m_line.push_back('\0');

    unsigned int i = 0;
    unsigned int len = static_cast<unsigned int>(m_line.size() - 1);
    while (i < len)
    {
        while (i < len && isspace(static_cast<unsigned char>(m_line[i]))) ++i;
        if (i >= len) break;
        m_offsets.push_back(i);
        std::vector<char> token;
        bool quoted = m_line[i] == '"';
        if (quoted) ++i;
        for (; i < len; ++i)
        {
            char c = m_line[i];
            if (quoted)
            {
                if (c == '"') { ++i; break; }
            }
            else if (isspace(static_cast<unsigned char>(c))) break;
            token.push_back(c);
        }
        token.push_back('\0');
        m_tokens.push_back(token);
    }
}

int TokenLine::CountToken() const { return static_cast<int>(m_tokens.size()); }

char *TokenLine::GetToken(int index)
{
    static char empty[] = "";
    if (index < 0 || static_cast<unsigned int>(index) >= m_tokens.size()) return empty;
    return &m_tokens[index][0];
}

char *TokenLine::GetRestOfLine(int index)
{
    static char empty[] = "";
    if (index < 0 || static_cast<unsigned int>(index) >= m_offsets.size()) return empty;
    return &m_line[m_offsets[index]];
}
