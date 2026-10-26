#ifndef GAMMASOURCE_SHARED_TOKENLINE_H
#define GAMMASOURCE_SHARED_TOKENLINE_H

#include <vector>

// A mutable line tokenizer for the legacy AdminServer RCON protocol.
// Quoted fields form one token; accessors are valid until the next SetLine.
class TokenLine
{
public:
    TokenLine();
    explicit TokenLine(const char *line);
    void SetLine(const char *line);
    int CountToken() const;
    char *GetToken(int index);
    char *GetRestOfLine(int index);

private:
    TokenLine(const TokenLine &);
    TokenLine &operator=(const TokenLine &);
    std::vector<char> m_line;
    std::vector< std::vector<char> > m_tokens;
    std::vector<unsigned int> m_offsets;
};

#endif
