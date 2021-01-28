/*
    srcFacts.cpp

    Produces a report with various counts of the number of 
    statements, declarations, etc. of a source code project
    in C++, C, Java, and C#.

    Input is an XML file in the srcML format.

    Code includes an almost-complete XML parser. Limitations:
    * DTD declarations are not handled
    * Well-formedness is not checked
*/

#include <iostream>
#include <iterator>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <errno.h>
#include <vector>
#include <algorithm>
#include <ctype.h>
#include <stdlib.h>

#if !defined(_MSC_VER)
#include <sys/uio.h>
#include <unistd.h>
#define READ read
#else
#include <BaseTsd.h>
#include <io.h>
typedef SSIZE_T ssize_t;
#define READ _read
#endif

#include <vector>

const int BUFFER_SIZE = 16 * 16 * 4096;

/*
    Refill the buffer preserving the unused data.
    Characters [pc, buffer.end()) are shifted left and new data
    is added to the rest of the buffer.

    @param pc Iterator to current position in buffer
    @param buffer Container for characters
    @param totalBytes Updated total bytes read
    @return Iterator to beginning of refilled buffer
*/
std::string::const_iterator refillBuffer(std::string::const_iterator pc, std::string& buffer, long& totalBytes) {

    // find number of unprocessed characters [pc, buffer.cend())
    auto d = std::distance(pc, buffer.cend());

    // move unprocessed characters, [pc, buffer.cend()), to start of the buffer
    std::copy(pc, buffer.cend(), buffer.begin());

    // read in trying to read whole blocks
    ssize_t numbytes = 0;
    while (((numbytes = READ(0, (void*)(buffer.data() + d), (size_t)(BUFFER_SIZE - d))) == (ssize_t) -1) &&
        (errno == EINTR)) {
    }
    // error in read
    if (numbytes == -1)
        return buffer.cend();
    // EOF
    if (numbytes == 0)
        return buffer.cend();

    if ((long) numbytes + d < buffer.size())
        buffer.resize(numbytes + d);

    // update with number of bytes read
    totalBytes += (long) numbytes;

    // return iterator to first part of buffer
    return buffer.cbegin();
}

int main() {
    std::string url;
    int textsize = 0;
    int loc = 0;
    int expr_count = 0;
    int function_count = 0;
    int class_count = 0;
    int file_count = 0;
    int decl_count = 0;
    int comment_count = 0;
    int depth = 0;
    long total = 0;
    bool intag = false;
    std::string buffer(BUFFER_SIZE, ' ');
    auto pc = buffer.cend();
    while (true) {
        if (std::distance(pc, buffer.cend()) < 5) {
            // refill buffer and adjust iterator
            pc = refillBuffer(pc, buffer, total);
            if (pc == buffer.cend())
                break;
        } else if (*pc == '<' && *std::next(pc) == '?') {
            // parse XML declaration
            std::string::const_iterator endpc = std::find(pc, buffer.cend(), '>');
            if (endpc == buffer.cend()) {
                pc = refillBuffer(pc, buffer, total);
                endpc = std::find(pc, buffer.cend(), '>');
                if (endpc == buffer.cend()) {
                    std::cerr << "parser error: Incomplete XML declaration\n";
                    return 1;
                }
            }
            std::advance(pc, strlen("<?xml"));
            pc = std::find_if_not(pc, endpc, [] (char c) { return isspace(c); });
            // parse required version
            if (pc == endpc) {
                std::cerr << "parser error: Incomplete version in XML declaration\n";
                return 1;
            }
            std::string::const_iterator pnameend = std::find(pc, endpc, '=');
            const std::string attr(pc, pnameend);
            pc = pnameend;
            std::advance(pc, 1);
            char delim = *pc;
            if (delim != '"' && delim != '\'') {
                std::cerr << "parser error: Invalid start delimiter for version in XML declaration\n";
                return 1;
            }
            std::advance(pc, 1);
            std::string::const_iterator pvalueend = std::find(pc, endpc, delim);
            if (pvalueend == endpc) {
                std::cerr << "parser error: Invalid end delimiter for version in XML declaration\n";
                return 1;
            }
            if (attr != "version") {
                std::cerr << "parser error: Missing required version in XML declaration\n";
                return 1;
            }
            const std::string version(pc, pvalueend);
            pc = std::next(pvalueend);
            pc = std::find_if_not(pc, endpc, [] (char c) { return isspace(c); });
            // parse encoding
            if (pc == buffer.cend())
                return 1;
            pnameend = std::find(pc, endpc, '=');
            const std::string attr2(pc, pnameend);
            pc = pnameend;
            std::advance(pc, 1);
            char delim2 = *pc;
            if (delim2 != '"' && delim2 != '\'')
                return 1;
            std::advance(pc, 1);
            pvalueend = std::find(pc, endpc, delim2);
            if (pvalueend == endpc)
                return 1;
            if (attr2 != "encoding") {
                 std::cerr << "parser error: Missing required encoding in XML declaration\n";
                 return 1;
            }
            const std::string encoding(pc, pvalueend);
            pc = std::next(pvalueend);
            pc = std::find_if_not(pc, endpc, [] (char c) { return isspace(c); });
            // parse standalone
            if (pc == endpc) {
                return 1;
            }
            pnameend = std::find(pc, endpc, '=');
            const std::string attr3(pc, pnameend);
            pc = pnameend;
            std::advance(pc, 1);
            char delim3 = *pc;
            if (delim3 != '"' && delim3 != '\'')
                return 1;
            std::advance(pc, 1);
            pvalueend = std::find(pc, endpc, delim3);
            if (pvalueend == endpc)
                return 1;
            if (attr3 != "standalone")
                return 1;
            const std::string standalone(pc, pvalueend);
            pc = std::next(pvalueend);
            pc = std::find_if_not(pc, endpc, [] (char c) { return isspace(c); });
            std::advance(pc, strlen("?>"));
            pc = std::find_if_not(pc, buffer.cend(), [] (char c) { return isspace(c); });

        } else if (*pc == '<' && *std::next(pc) == '/') {
            // parse end tag
            --depth;
            std::advance(pc, 2);
            std::string::const_iterator pnameend = std::find_if(pc, buffer.cend(), [] (char c) { return isspace(c) || c == '>' || c == '/'; });
            if (pnameend == buffer.cend()) {
                pc = refillBuffer(pc, buffer, total);
                pnameend = std::find_if(pc, buffer.cend(), [] (char c) { return isspace(c) || c == '>' || c == '/'; });
                if (pnameend == buffer.cend())
                    return 1;
            }
            const std::string qname(pc, pnameend);
            const auto colonpos = qname.find(':');
            std::string prefixbase;
            if (colonpos != std::string::npos)
                prefixbase = qname.substr(0, colonpos);
            const std::string prefix = std::move(prefixbase);
            std::string local_namebase;
            if (colonpos != std::string::npos)
                local_namebase = qname.substr(colonpos + 1);
            else
                local_namebase = qname;
            const std::string local_name = std::move(local_namebase);
            pc = pnameend;
            std::string::const_iterator endpc = std::find(pc, buffer.cend(), '>');
            if (endpc == buffer.cend()) {
                pc = refillBuffer(pc, buffer, total);
                endpc = std::find(pc, buffer.cend(), '>');
                if (endpc == buffer.cend())
                    return 1;
            }
            pc = std::next(endpc);

        } else if (*pc == '<' && *std::next(pc) != '/' && *std::next(pc) != '?') {
            // parse start tag
            std::advance(pc, 1);
            std::string::const_iterator pnameend = std::find_if(pc, buffer.cend(), [] (char c) { return isspace(c) || c == '>' || c == '/'; });
            if (pnameend == buffer.cend()) {
                pc = refillBuffer(pc, buffer, total);
                pnameend = std::find_if(pc, buffer.cend(), [] (char c) { return isspace(c) || c == '>' || c == '/'; });
                if (pnameend == buffer.cend()) {
                    std::cerr << "parser error : Unterminated start tag '" << std::string(pc, pnameend) << "'\n";
                    return 1;
                }
            }
            const std::string qname(pc, pnameend);
            const auto colonpos = qname.find(':');
            std::string prefixbase;
            if (colonpos != std::string::npos)
                prefixbase = qname.substr(0, colonpos);
            const std::string prefix = std::move(prefixbase);
            std::string local_namebase;
            if (colonpos != std::string::npos)
                local_namebase = qname.substr(colonpos + 1);
            else
                local_namebase = qname;
            const std::string local_name = std::move(local_namebase);
            pc = pnameend;
            if (local_name == "expr")
                ++expr_count;
            else if (local_name == "function")
                ++function_count;
            else if (local_name == "decl")
                ++decl_count;
            else if (local_name == "class")
                ++class_count;
            else if (local_name == "unit" && depth > 1)
                ++file_count;
            else if (local_name == "comment")
                ++comment_count;
            pc = std::find_if_not(pc, buffer.cend(), [] (char c) { return isspace(c); });
            ++depth;
            intag = true;
            if (intag && *pc == '>') {
                std::advance(pc, 1);
                intag = false;
            }
            if (intag && *pc == '/' && *std::next(pc) == '>') {
                std::advance(pc, 2);
                intag = false;
                --depth;
            }
            if (intag) {
                std::string::const_iterator endpc = std::find(pc, buffer.cend(), '>');
                if (endpc == buffer.cend()) {
                    pc = refillBuffer(pc, buffer, total);
                    endpc = std::find(pc, buffer.cend(), '>');
                }
                if (endpc == buffer.cend())
                    return 1;
            }
        } else if (intag && *pc != '>' && *pc != '/' && (int) std::distance(pc, buffer.cend()) > strlen("xmlns") && std::string(pc, std::next(pc, strlen("xmlns"))) == "xmlns"
            && (*std::next(pc, strlen("xmlns")) == ':' || *std::next(pc, strlen("xmlns")) == '=')) {
            // parse namespace
            std::advance(pc, strlen("xmlns"));
            std::string::const_iterator pnameend = std::find(pc, buffer.cend(), '=');
            if (pnameend == buffer.cend())
                return 1;
            pc = pnameend;
            std::string prefix;
            if (*pc == ':') {
                std::advance(pc, 1);
                prefix.assign(pc, pnameend);
            }
            pc = std::next(pnameend);
            pc = std::find_if_not(pc, buffer.cend(), [] (char c) { return isspace(c); });
            if (pc == buffer.cend()) {
                pc = refillBuffer(pc, buffer, total);
                pc = std::find_if_not(pc, buffer.cend(), [] (char c) { return isspace(c); });
                if (pc == buffer.cend())
                    return 1;
            }
            char delim = *pc;
            if (delim != '"' && delim != '\'')
                return 1;
            std::advance(pc, 1);
            std::string::const_iterator pvalueend = std::find(pc, buffer.cend(), delim);
            if (pvalueend == buffer.cend()) {
                pc = refillBuffer(pc, buffer, total);
                pvalueend = std::find(pc, buffer.cend(), delim);
                if (pvalueend == buffer.cend())
                    return 1;
            }
            const std::string uri(pc, pvalueend);
            pc = std::next(pvalueend);
            pc = std::find_if_not(pc, buffer.cend(), [] (char c) { return isspace(c); });
            if (intag && *pc == '>') {
                std::advance(pc, 1);
                intag = false;
            }
            if (intag && *pc == '/' && *std::next(pc) == '>') {
                std::advance(pc, 2);
                intag = false;
            }
        } else if (intag && *pc != '>' && *pc != '/') {
            // parse attribute
            std::string::const_iterator pnameend = std::find(pc, buffer.cend(), '=');
            if (pnameend == buffer.cend())
                return 1;
            const std::string qname(pc, pnameend);
            const auto colonpos = qname.find(':');
            const std::string prefix     = colonpos != std::string::npos ? qname.substr(0, colonpos) : std::string("");
            const std::string local_name = colonpos != std::string::npos ? qname.substr(colonpos + 1) : qname;
            pc = std::next(pnameend);
            pc = std::find_if_not(pc, buffer.cend(), [] (char c) { return isspace(c); });
            if (pc == buffer.cend())
                return 1;
            char delim = *pc;
            if (delim != '"' && delim != '\'') {
                std::cerr << "parser error : attribute " << qname << " missing delimiter\n";
                return 1;
            }
            std::advance(pc, 1);
            std::string::const_iterator pvalueend = std::find(pc, buffer.cend(), delim);
            if (pvalueend == buffer.cend())
                return 1;
            const std::string value(pc, pvalueend);
            if (local_name == "url")
                url = value;
            pc = std::next(pvalueend);
            pc = std::find_if_not(pc, buffer.cend(), [] (char c) { return isspace(c); });
            if (intag && *pc == '>') {
                std::advance(pc, 1);
                intag = false;
            }
            if (intag && *pc == '/' && *std::next(pc) == '>') {
                std::advance(pc, 2);
                intag = false;
            }

        } else if (*pc == '<' && *std::next(pc) == '!' && *std::next(pc, 2) == '[') {
            // parse CDATA
            const std::string endcdata = "]]>";
            std::advance(pc, strlen("<![CDATA["));
            std::string::const_iterator endpc = std::search(pc, buffer.cend(), endcdata.begin(), endcdata.end());
            if (endpc == buffer.cend()) {
                pc = refillBuffer(pc, buffer, total);
                endpc = std::search(pc, buffer.cend(), endcdata.begin(), endcdata.end());
                if (endpc == buffer.cend())
                    return 1;
            }
            std::string characters(pc, endpc);
            textsize += (int) characters.size();
            loc += (int) std::count(characters.begin(), characters.end(), '\n');
            pc = std::next(endpc, strlen("]]>"));
        } else if (*pc == '<' && *std::next(pc) == '!' && *std::next(pc, 2) == '-' && *std::next(pc, 3) == '-') {
            // parse XML comment
            const std::string endcomment = "-->";
            std::string::const_iterator endpc = std::search(pc, buffer.cend(), endcomment.begin(), endcomment.end());
            if (endpc == buffer.cend()) {
                pc = refillBuffer(pc, buffer, total);
                endpc = std::search(pc, buffer.cend(), endcomment.begin(), endcomment.end());
                if (endpc == buffer.cend())
                    return 1;
            }
            pc = std::next(endpc, strlen("-->"));
            pc = std::find_if_not(pc, buffer.cend(), [] (char c) { return isspace(c); });
        } else if (*pc != '<' && depth == 0) {
            // parse characters before or after XML
            pc = std::find_if_not(pc, buffer.cend(), [] (char c) { return isspace(c); });
            if (pc == buffer.cend() || !isspace(*pc)) {
                std::cerr << "parser error : Start tag expected, '<' not found\n";
                return 1;
            }
        } else if (*pc == '&') {
            // parse entity references
            std::string characters;
            if (std::distance(pc, buffer.cend()) < 3) {
                pc = refillBuffer(pc, buffer, total);
                if (std::distance(pc, buffer.cend()) < 3) {
                    std::string partialEntity(pc, buffer.cend());
                    std::cerr << "parser error : Incomplete entity reference, '" << partialEntity << "'\n";
                    return 1;
                }
            }
            if (*std::next(pc) == 'l' && *std::next(pc, 2) == 't' && *std::next(pc, 3) == ';') {
                characters += '<';
                std::advance(pc, strlen("&lt;"));
            } else if (*std::next(pc) == 'g' && *std::next(pc, 2) == 't' && *std::next(pc, 3) == ';') {
                characters += '>';
                std::advance(pc, strlen("&gt;"));
            } else if (*std::next(pc) == 'a' && *std::next(pc, 2) == 'm' && *std::next(pc, 3) == 'p') {
                if (std::distance(pc, buffer.cend()) < 4) {
                    pc = refillBuffer(pc, buffer, total);
                    if (std::distance(pc, buffer.cend()) < 4) {
                        std::string partialEntity(pc, buffer.cend());
                        std::cerr << "parser error : Incomplete entity reference, '" << partialEntity << "'\n";
                        return 1;
                    }
                }
                if (*std::next(pc, 4) != ';') {
                    std::string partialEntity(pc, std::next(pc, 4));
                    std::cerr << "parser error : Incomplete entity reference, '" << partialEntity << "'\n";
                    return 1;
                }
                characters += '&';
                std::advance(pc, strlen("&amp;"));
            } else {
                characters += '&';
                std::advance(pc, 1);
            }
            textsize += (int) characters.size();
        } else if (*pc != '<') {
            // parse characters
            std::string::const_iterator endpc = std::find_if(pc, buffer.cend(), [] (char c) { return c == '<' || c == '&'; });
            std::string characters(pc, endpc);
            loc += (int) std::count(characters.cbegin(), characters.cend(), '\n');
            textsize += (int) characters.size();
            pc = endpc;
        }
    }
    std::cout << "# srcFacts: " << url << '\n';
    std::cout << "| Item | Count |\n";
    std::cout << "|:-----|-----:|\n";
    std::cout << "| srcML | " << total << " |\n";
    std::cout << "| files | " << file_count << " |\n";
    std::cout << "| LOC | " << loc << " |\n";
    std::cout << "| characters | " << textsize << " |\n";
    std::cout << "| classes | " << class_count << " |\n";
    std::cout << "| functions | " << function_count << " |\n";
    std::cout << "| declarations | " << decl_count << " |\n";
    std::cout << "| expressions | " << expr_count << " |\n";
    std::cout << "| comments | " << comment_count << " |\n";
    return 0;
}