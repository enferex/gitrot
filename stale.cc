#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <cerrno>
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
    BlankBlock() { _name = "Blank Block"; }
};

class CommentBlock : public Block
{
public:
    CommentBlock() { _name = "Comment Block"; }
};

class CodeBlock : public Block
{
public:
    CodeBlock() { _name = "Code Block"; }
};

class TranslationFile
{
public:
    TranslationFile(const char *fname);
    static Block *createBlock(LinesItr &itr);

private:
    void parse(ifstream &fs);
    string _fname;
    Blocks _blocks;

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
    if (feof(fp) || ferror(fp))
      return NULL;

    Line *ln = new Line();

    // <40-byte sha> <orig line number> <final line number> <lines in group>
    ln->_header = getTextLine(fp);

    ln->_author      = getTextLine(fp);
    ln->_author_mail = getTextLine(fp);
    ln->_author_time = getTextLine(fp);
    ln->_author_tz   = getTextLine(fp);
    
    ln->_commiter      = getTextLine(fp);
    ln->_commiter_mail = getTextLine(fp);
    ln->_commiter_time = getTextLine(fp);
    ln->_commiter_tz   = getTextLine(fp);

    ln->_summary          = getTextLine(fp);
    ln->_prev_or_boundary = getTextLine(fp);
    ln->_filename         = getTextLine(fp);
    ln->_content          = getTextLine(fp);
    ln->_lineno           = Line::incrLineCounter();

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

// Continusly read lines grouping based on
// comment and code.
Block *TranslationFile::createBlock(LinesItr &itr)
{
    Block *block = NULL;

    if (Line::isBlank(itr)) {
        // While we are reading blank lines...
        block = new BlankBlock();
        while (*itr && Line::isBlank(itr)) {
            block->addLine(*itr);
            ++itr;
        }
    }
    else if (Line::hasUnterminatedComment(itr)) {
        // While we are in a comment block...
        block = new CommentBlock();
        while (*itr && Line::findCommentBlockEnd(itr) != string::npos) {
            block->addLine(*itr);
            ++itr;
        }
    }
    else {
        // While we do not find a comment block...
        block = new CodeBlock();
        while (*itr && Line::findCommentBlockBegin(itr) == string::npos) {
            block->addLine(*itr);
            ++itr;
        }
    } 

    return block;
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
        fp = popen(("git blame --line-porcelain " + file).c_str(), "r");
    }
    else // No path, just use filename 
      fp = popen(("git blame --line-porcelain " + _fname).c_str(), "r");

    // Now we have file handle, obtain one string per line
    Lines lines;
    while (Line *line = Line::getLine(fp))
      lines.push_back(line);

    // File has been read in, now put the lines into blocks
    for (auto i=lines.begin(); *i && i!=lines.end(); ++i) {
        Block *blk = createBlock(i);
        if (blk)
          this->_blocks.push_back(blk);
    }

    pclose(fp);
}

TranslationFile::TranslationFile(const char *fname)
{
    _fname = fname;

    cout << "Initializing " << fname << endl;

    ifstream fs(fname);
    if (!fs.good())
      cerr << "Could not open file " << fname << ": " 
           << " Error(" << errno << ") " << strerror(errno);
    else
      parse(fs);
}

int main(int argc, char **argv)
{
    vector<TranslationFile> files;

    for (int i=1; i<argc; ++i) {
        TranslationFile tf(argv[i]);
        files.push_back(tf);
    }

#ifdef DEBUG
    for_each(files.cbegin(), files.cend(),
             [](TranslationFile t) { cout << t << endl; });
#endif

    return 0;
}
