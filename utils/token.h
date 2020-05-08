#ifndef __CTOKEN_H__
#define __CTOKEN_H__

#include <vector>
#include <string>
#include <sstream>

class CToken {
    std::vector<std::string> m_tokens;
    std::string              m_delimiters;

    static std::string       emptyStr;

    friend std::ostream &operator<<(std::ostream &out, const CToken &token);
public:

    CToken() {}

    explicit CToken(const char *delimiter)
                        : m_delimiters(delimiter) {}

    CToken(const char *delimiter, const std::string &input)
                        : m_delimiters(delimiter) {
            tokenize(input);
    }

    const std::vector<std::string>::const_iterator begin() const { return m_tokens.begin(); }
    const std::vector<std::string>::const_iterator end() const { return m_tokens.end(); }
    
    void SetDelimiter(const char *delimiter) {
        m_delimiters = delimiter;
    }

    void tokenize(const std::string &input, const char *delimiter = NULL);

    /// restituisce l'ultimo token
    const std::string &last() { return m_tokens.back(); }

    /**
     * restituisce i token in ordine inverso, dall'ultimo (indice 0),
     * verso il primo, (indice size - 1).
     */
    const std::string &reverse(size_t index);

    size_t  size()  const { return m_tokens.size(); }
    bool    empty() const { return m_tokens.empty(); }
    void    clear() { m_tokens.clear(); }

    const std::string & operator[](size_t size) const;
};

inline std::ostream &
operator<<(std::ostream &out, const CToken &token)
{
    for (size_t i = 0; i < token.size(); ++i) {
#ifdef CTOKEN_TEST
        out << "<" << token[i] << ">" << std::endl;
#else
        out << token[i] << std::endl;
#endif
    }

    return out;
}

#endif //__CTOKEN_H__
