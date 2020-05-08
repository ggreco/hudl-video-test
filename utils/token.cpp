#include "token.h"
#include <iostream>

std::string CToken::emptyStr;

void CToken::
tokenize(const std::string &input, const char *delimiter) {
    std::string::size_type start = 0;
    std::string::size_type end = 0;

    if (delimiter)
        SetDelimiter(delimiter);

    //m_tokens.clear();
    clear();

    //while ((end = input.find_first_of(m_delimiters, start))
    while ((end = input.find(m_delimiters, start))
                                           != std::string::npos) {

        m_tokens.push_back(input.substr(start, end-start));
        //start = end + 1;
        start = end + m_delimiters.size(); 
    }

    m_tokens.push_back(input.substr(start));
}

/*
int Darts::CToken::
extract(std::string &str) {
	int pos;
     
	if ((pos = str.find_first_of(m_delimiters)) != std::string::npos) {
		//string tmp = str.substr(0, pos);
		m_tokens.push_back(str.substr(0, pos));
		str = str.substr(pos+1);
		return 0;
	} else {
        //m_tokens[m_howmany++] = str;
		m_tokens.push_back(str);
		return -1;
	}
}
*/

const std::string & CToken::
operator[](size_t size) const
{
    if (size >= m_tokens.size()) {
        //std::ostringstream os;
        //os << "CToken subscripting " << size << " too high";
        return emptyStr;
    }
    return m_tokens[size];
}

const std::string &CToken::
reverse(size_t index) {
	if (index >= m_tokens.size())
		return emptyStr;
	    
	return m_tokens[m_tokens.size() -1 - index];
}


#ifdef CTOKEN_TEST

#include <iostream>
#include <exception>

int
main(int argc, char *argv[])
{
    std::string input = "pippo | pluto | paperino | minnie";

    try {
    CToken token(" | ");

    token.tokenize(input);
    std::cout << token;

    CToken token2("|", input);
    std::cout << token2;

    token.tokenize(input, "| ");
    std::cout << token;

    } catch (const std::string &excp) {
        std::cout << "Catch Exception: " << excp << std::endl;
    } catch (std::exception &excp) {
        std::cout << "Catch Exception: " << excp.what() << std::endl;
    } catch (...) {
        std::cout << "Catch Exception: ..." << std::endl;
    }
}

#endif
