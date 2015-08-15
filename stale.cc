#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <cerrno>
#include <getopt.h>
#include <cstdint>
#include <cstring>
using std::cerr;
using std::cout;
using std::endl;
using std::for_each;
using std::ifstream;
using std::ostream;
using std::string;
using std::vector;

#define COMMENT_BLOCK_BEGIN "/*"
#define COMMENT_BLOCK_END   "*/"
#define BLAME               "git blame --line-porcelain "

class Block;
class Line;
typedef vector<Line *> Lines;
typedef Lines::iterator LinesItr;

class Line
{
public:
    const string getContent() const { return _content; }

    // Parsing utils
    static string getTextLine(FILE *fp);
    static Line *getLine(FILE *fp);
    static bool isBlank(LinesItr &itr);
    static bool justComment(LinesItr &itr);
    static bool hasUnterminatedComment(LinesItr &itr);
    static size_t findCommentBlockBegin(string str, size_t pos);
    static size_t findCommentBlockBegin(LinesItr &itr);
    static size_t findCommentBlockEnd(string str, size_t pos);
    static size_t findCommentBlockEnd(LinesItr &itr);
    static unsigned incrLineCounter(void) { return ++Line::_all_line_count; }

private:
    // git help blame 
    string _header;
    string _author;
    string _author_mail;
    string _author_time;
    string _author_tz;
    string _commiter;
    string _commiter_mail;
    string _commiter_time;
    string _commiter_tz;
    string _summary;
    string _prev_or_boundary;
    string _filename;
    string _content;
    unsigned _lineno;
    static unsigned _all_line_count;

    friend ostream &operator<<(ostream &os, const Line &line) {
        return os << line._lineno << line._content;
    }
};


// Global keeping track of line numbers
unsigned Line::_all_line_count = 0;

class Block
{
public:
    Block() { _name = "Unknown Block"; }
    const char *getName() const { return _name; }
    const Lines &getLines() const { return _lines; }
    void addLine(Line *line) { _lines.push_back(line); }

protected:
    const char *_name;

private:
    Lines _lines;
};
typedef vector<Block *> Blocks;

class BlankBlock : public Block
{
public:
    BlankBlock(LinesItr &itr);
};

class CommentBlock : public Block
{
public:
    CommentBlock(LinesItr &itr);
};

class CodeBlock : public Block
{
public:
    CodeBlock(LinesItr &itr);
};

class TranslationFile
{
public:
    TranslationFile(const char *fname);
    void createBlock(LinesItr &itr);
    const string getName() const { return _fname; }

    // Counters (for stats)
    size_t nLines() const { return _n_lines; }
    size_t nCodeBlocks() const { return _n_code_blocks; }
    size_t nBlankBlocks() const { return _n_blank_blocks; }
    size_t nCommentBlocks() const { return _n_comment_blocks; }

private:
    void parse(ifstream &fs);
    string _fname;
    Blocks _blocks;

    // Stats
    size_t _n_lines;
    size_t _n_code_blocks;
    size_t _n_blank_blocks;
    size_t _n_comment_blocks;

    friend ostream &operator<<(ostream &os, const TranslationFile tf) { 
        os << "***** " << tf._fname << " *****" << endl;
        int cnt = 0;
        for (auto i=tf._blocks.cbegin(); i!=tf._blocks.cend(); ++i) {
             auto lines = (*i)->getLines();
             os << "==> " << (*i)->getName() << " " << cnt++ << " ("
                << lines.size() << " lines):" << endl;
             for (auto j=lines.cbegin(); j!=lines.cend(); ++j)
                 os << **j << endl;
        }
        return os;
    }
};

string Line::getTextLine(FILE *fp)
{
    char c = '\0';
    string str;
    while (c != '\n' && !feof(fp) && !ferror(fp)) {
      c = fgetc(fp);
      str += c;
    }
    return str.size() ? str : "";
}

// Get one line (made up from multiple lines of git blame
Line *Line::getLine(FILE *fp)
{
    Line *ln = new Line();

    // <40-byte sha> <orig line number> <final line number> <lines in group>
    ln->_header = getTextLine(fp);

    // Previous getTextLine did the initial read, check file status.
    if (feof(fp) && !ferror(fp)) {
        delete ln;
        return NULL;
    }
    else if (ferror(fp)) {
        cerr << "Error reading git blame information." << endl;
        return NULL;
    }

    ln->_author        = getTextLine(fp);
    ln->_author_mail   = getTextLine(fp);
    ln->_author_time   = getTextLine(fp);
    ln->_author_tz     = getTextLine(fp);
    ln->_commiter      = getTextLine(fp);
    ln->_commiter_mail = getTextLine(fp);
    ln->_commiter_time = getTextLine(fp);
    ln->_commiter_tz   = getTextLine(fp);
    ln->_summary       = getTextLine(fp);

    // Handle the previous or boundary string if it is in place else it is the
    // filename string.
    string tmp = getTextLine(fp);
    if (tmp[0] != 'f') // Not 'f'ilename
    {
        ln->_prev_or_boundary = tmp;
        ln->_filename         = getTextLine(fp);
    }
    else
      ln->_filename = tmp;

    ln->_content = getTextLine(fp);
    ln->_lineno  = Line::incrLineCounter();

    // Remove newline
    if (ln->_content.size())
      ln->_content.pop_back();

    return ln;
}

size_t Line::findCommentBlockBegin(string str, size_t pos)
{
    bool in_string = false;
    bool in_comment = false;
    char prev = '\0';

    for (auto i=str.begin()+pos; i!=str.end(); ++i) {
        char c = *i;
        if (c == '"')  {
          in_string ^= in_string;
          in_comment = false;
        }
        else if (!in_string && (c == '/') && (prev == '/'))
          in_comment = true;
        else if (!in_string && (c == '*') && (prev == '/'))
          in_comment = true;
        
        if (in_comment)
            return i - str.begin();
        prev = c;
    }

    return string::npos;
}

size_t Line::findCommentBlockEnd(string str, size_t pos)
{
    bool in_string = false;
    char prev = '\0';

    for (auto i=str.begin()+pos; i!=str.end(); ++i) {
        char c = *i;
        if (c == '"')
          in_string ^= in_string;
        else if (!in_string && (c == '/') && (prev == '*'))
          return i - str.begin();
        prev = c;
    }

    return string::npos;
}

size_t Line::findCommentBlockEnd(LinesItr &itr)
{
    return Line::findCommentBlockEnd((*itr)->_content, 0);
}

size_t Line::findCommentBlockBegin(LinesItr &itr)
{
    return Line::findCommentBlockBegin((*itr)->_content, 0);
}

// Start of a comment block
bool Line::hasUnterminatedComment(LinesItr &itr)
{
    string content = (*itr)->_content;
    bool in_comment = false;
    size_t pos = 0;

    while ((pos = findCommentBlockBegin(content, pos)) != string::npos) {
        in_comment = true;
        if ((pos = Line::findCommentBlockEnd(content, pos)) != string::npos)
          in_comment = false;
        if (pos == string::npos)
          break;
    }

    return in_comment;
}

// Just spaces
bool Line::isBlank(LinesItr &itr)
{
    for (auto i=(*itr)->_content.cbegin(); i!=(*itr)->_content.cend(); ++i)
      if (!isspace(*i))
        return false;

    return true;
}

// Just a comment line
bool Line::justComment(LinesItr &itr)
{
    char prev = '\0';

    for (auto i=(*itr)->_content.cbegin(); i!=(*itr)->_content.cend(); ++i) {
        if (isspace(*i))
            continue;
        else if (*i == '/' && prev == '/')
            return true;
        // Look for inline /* */ comments and nothing after
        else if (*i == '*' && prev == '/') {
            auto str  = (*itr)->_content;
            auto end1 = str.find_last_of("/");
            auto end2 = str.find_last_not_of("/ \n\t\r");
            if (end2 != string::npos && end2 != string::npos && end2 > end1)
              return false; // Non comment characters after '/'
            else if (end1 != string::npos && (str.at(end1 - 1) == '*') && 
                     (end2 == string::npos || end2 < end1))
              return true;
        }
        prev = *i;
    }

    return false;
}

BlankBlock::BlankBlock(LinesItr &itr)
{
    _name = "Blank Block";
    while (*itr && Line::isBlank(itr)) {
        addLine(*itr);
        ++itr;
    }
}

CommentBlock::CommentBlock(LinesItr &itr)
{
    _name = "Comment Block";

    if (Line::justComment(itr)) { 
        // While we keep seeing inline "//" comments and no code
        while (*itr && Line::justComment(itr)) {
            addLine(*itr);
            ++itr;
        }
    }
    else if (Line::hasUnterminatedComment(itr)) {
        // While we are in a comment block...
        size_t end = string::npos;
        while (*itr && !Line::isBlank(itr)) {
            end = Line::findCommentBlockEnd(itr);
            if (end == string::npos) { // No end found, and in comment block
                addLine(*itr);
                ++itr;
            } 
            else if (Line::findCommentBlockEnd(itr) != string::npos) {
                addLine(*itr);
                ++itr;
                break;
            }
        }
    }
}

CodeBlock::CodeBlock(LinesItr &itr)
{
    _name = "Code Block";

    // While we do not find a comment block...
    while (*itr && !Line::isBlank(itr) &&
        Line::findCommentBlockBegin(itr) == string::npos) {
        addLine(*itr);
        ++itr;
    }
}

// Continusly read lines grouping based on
// comment and code.
void TranslationFile::createBlock(LinesItr &itr)
{
    Block *block = NULL;
    const LinesItr orig = itr;

    if (Line::isBlank(itr)) {
        if ((block = new BlankBlock(itr)))
          ++_n_blank_blocks;
    }
    else if (Line::justComment(itr) ||  Line::hasUnterminatedComment(itr)) {
        if ((block = new CommentBlock(itr)))
          ++_n_comment_blocks;
    }
    else {
        if ((block = new CodeBlock(itr)))
          ++_n_code_blocks;
    }

    // If the orig iter is not the current iter, then we iterated one past
    // to fail in a while condition.
    if (orig != itr)
      --itr;

    if (block)
      _n_lines += block->getLines().size();

    _blocks.push_back(block);
}

void TranslationFile::parse(ifstream &fs)
{
    FILE *fp;
    string path;
   
    // Get path and cd to it  
    auto pos = _fname.find_last_of('/');
    if ((pos != string::npos) && pos) {
        path = _fname.substr(0, pos);
        string file = _fname.substr(pos + 1);
        chdir(path.c_str());
        fp = popen((BLAME + file).c_str(), "r");
    }
    else // No path, just use filename 
      fp = popen((BLAME + _fname).c_str(), "r");

    // Now we have file handle, obtain one string per line
    Lines lines;
    while (Line *line = Line::getLine(fp))
      lines.push_back(line);

    if (lines.size() == 0) {
        cerr << "Did not find any git blame information." << endl
             << "Has this file been committed to your git repository?"
             << endl;
        return;
    }

    // File has been read in, now put the lines into blocks
    for (auto i=lines.begin(); *i && i!=lines.end(); ++i)
        this->createBlock(i);

    pclose(fp);
}

TranslationFile::TranslationFile(const char *fname)
{
    // Initialize members
    cout << "Initializing " << fname << endl;
    _fname = fname;
    _n_lines = _n_code_blocks = _n_blank_blocks =  _n_comment_blocks = 0;

    ifstream fs(fname);
    if (!fs.good())
      cerr << "Could not open file " << fname << ": " 
           << " Error(" << errno << ") " << strerror(errno);
    else
      parse(fs);
}

static void usage(const char *execname)
{
    cout << "Usage: " << execname << " [-h] [-r <num>] [FILE ...]" << endl
         << "  -r <num>: Range in 'num' days between code and comment " << endl
         << "            block modification times to which the comment " << endl
         << "            is considered  stale." << endl
         << "  -v:       Verbose output." << endl
         << "  -s:       Stats output." << endl
         << "  -h:       This help message." << endl
         << "  FILE:     File path to a git committed file to analyize."
         << endl;
}

int main(int argc, char **argv)
{
    int opt;
    vector<TranslationFile> files;

    int range = 7;
    bool verbose = false, stats = false;
    while ((opt=getopt(argc, argv, "vsr:h")) != -1) {
        switch (opt) {
        case 'v': verbose = true; break;
        case 's': stats = true; break;
        case 'r': range = atoi(optarg); break;
        case 'h': usage(argv[0]); exit(EXIT_SUCCESS); break;
        default:
            cerr << "Invalid option: " << argv[optind] << endl;
            exit(EXIT_FAILURE);
        }
    }

    for (int i=optind; i<argc; ++i) {
        TranslationFile tf(argv[i]);
        files.push_back(tf);
    }

    if (verbose)
      for_each(files.cbegin(), files.cend(),
               [](TranslationFile t) { cout << t << endl; });

    if (stats) {
        cout << "Total Files: " << files.size() << endl;
        for_each(files.cbegin(), files.cend(),
                 [](TranslationFile t) { 
                      cout << t.getName() << endl
                           << "\tBlankBlocks   " << t.nBlankBlocks() << endl
                           << "\tCodeBlocks    " << t.nCodeBlocks() << endl
                           << "\tCommentBlocks " << t.nCommentBlocks() << endl
                           << "\tLines         " << t.nLines() << endl;         
                 });
    }

    return 0;
}
