/******************************************************************************
 * main.c
 *
 * gitrot - Stale comment locater
 *
 * Copyright (C) 2015, Matt Davis (enferex)
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *             
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more
 * details.
 *                             
 * You should have received a copy of the GNU
 * General Public License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <utility>
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
using std::pair;
using std::string;
using std::vector;

#define COMMENT_BLOCK_BEGIN "/*"
#define COMMENT_BLOCK_END   "*/"
#define BLAME               "git blame --line-porcelain "

class Block;
class Line;
typedef vector<Line *> Lines;
typedef Lines::iterator LinesItr;

enum block_type_e {
    UNKNOWN_BLOCK,
    COMMENT_BLOCK,
    CODE_BLOCK,
    BLANK_BLOCK,
    N_BLOCK_TYPES // Always last
};

class Line
{
public:
    const string getContent() const { return _content; }
    unsigned getLineNum() const { return _lineno; }
    uint64_t getAuthorTime() const;

    // Parsing utils
    static string getTextLine(FILE *fp, char skip_to='\0');
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
    // See `git help blame'
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

class Block;
typedef vector<Block *> Blocks;
typedef Blocks::iterator BlocksItr;
typedef pair<const Block*, const Block*> BlockPair;
typedef vector<BlockPair> BlockPairs;

static int block_ids; // Useful for debugging
class Block
{
public:
    Block() : _id(++block_ids), _name("Unknown Block") {;}

    void addLine(Line *line) { _lines.push_back(line); }
    int64_t getMostRecentlyUpdated() const;
    const char *getName() const { return _name; }
    block_type_e getType() const { return _type; }
    const Lines &getLines() const { return _lines; }
    unsigned getFirstLineNum() const { 
        return _lines.size() ? _lines[0]->getLineNum() : 0;
    }
    static unsigned getRangeDifference(const Block *first, const Block *second);

    int _id;

protected:
    const char *_name;
    block_type_e _type;

private:
    Lines _lines;
};

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
    const Blocks &getBlocks() const { return _blocks; }
    BlockPair nextCommentCode(
        BlocksItr &itr, const BlocksItr &end, unsigned range) const;

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
typedef vector<TranslationFile> TranslationFiles;

// Return author time in unix-timestamp seconds
uint64_t Line::getAuthorTime() const
{
    return std::stoull(_author_time, NULL);
}

string Line::getTextLine(FILE *fp, char skip_to)
{
    char c = '\0';
    string str;
    while (c != '\n' && !feof(fp) && !ferror(fp)) {
      c = fgetc(fp);
      str += c;
    }
    if (skip_to && str.find_first_of(skip_to) != string::npos)
        return str.substr(str.find_first_of(skip_to)+ 1);
    else
      return str;
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
    ln->_author_time   = getTextLine(fp, ' ');
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
    _type = BLANK_BLOCK;
    while (*itr && Line::isBlank(itr)) {
        addLine(*itr);
        ++itr;
    }
}

CommentBlock::CommentBlock(LinesItr &itr)
{
    _name = "Comment Block";
    _type = COMMENT_BLOCK;

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
    _type = CODE_BLOCK;

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
    cout << "Usage: " << execname << " [-s] [-v] [-h] [-r <num>] [FILE ...]" << endl
         << "  -r <num>: Range in 'num' days between code and comment " << endl
         << "            block modification times to which the comment " << endl
         << "            is considered  stale." << endl
         << "  -s:       Stats output." << endl
         << "  -v:       Verbose output." << endl
         << "  -h:       This help message." << endl
         << "  FILE:     File path to a git committed file to analyize."
         << endl;
}

// Returns days
unsigned Block::getRangeDifference(const Block *first, const Block *second)
{
    auto a = first->getMostRecentlyUpdated();
    auto b = second->getMostRecentlyUpdated();
    return std::abs(b - a) / (60 * 60 * 24); // Seconds to days
}

// Return the timestamp for the most recently updated line in this block
int64_t Block::getMostRecentlyUpdated() const
{
    const Line *recent = NULL;
    for (auto ll=_lines.cbegin(); ll!=_lines.cend(); ++ll) {
        if (!recent)
            recent = *ll;
        else if ((*ll)->getAuthorTime() > recent->getAuthorTime())
            recent = *ll;
    }
    return recent->getAuthorTime();
}

BlockPair TranslationFile::nextCommentCode(
    BlocksItr       &itr, 
    const BlocksItr &end,
    unsigned range) const
{
    BlockPair pr = std::make_pair<const Block*, const Block *>(NULL, NULL);

    if (itr == end)
        return pr;

    for ( ; itr!=end; ++itr) {
        if ((*itr)->getType() == COMMENT_BLOCK) {
            pr.first = *itr;
            break;
        }
    }

    if (!pr.first || itr == end)
        return pr;

    for ( ; itr!=end; ++itr) {
        if ((*itr)->getType() == CODE_BLOCK) {
            pr.second = *itr;
            break;
        }
    }

    if (itr == end)
      return pr;

    // Are these two blocks within range? Else recurse and look furthur
    auto days = Block::getRangeDifference(pr.first, pr.second);
    if (days >= range)
      return pr;
    else
      return nextCommentCode(++itr, end, range);
}

static BlockPairs find_ranges(const TranslationFile &file, unsigned range)
{
    BlockPairs in_range;
    Blocks blks = file.getBlocks();
    const BlocksItr end = blks.end();

    for (BlocksItr b = blks.begin(); b!=end; ++b) {
        BlockPair pr = file.nextCommentCode(b, end, range);
        if (!pr.first || !pr.second)
          break;
        in_range.push_back(pr);
    }

    return in_range;

}

int main(int argc, char **argv)
{
    int opt;
    TranslationFiles files;

    int range = 0;
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

    // Parse files
    for (int i=optind; i<argc; ++i) {
        TranslationFile tf(argv[i]);
        files.push_back(tf);
    }

    // Do any work... 
    if (range > 0) {
        for (auto tf=files.cbegin(); tf!=files.cend(); ++tf) {
            auto in_range = find_ranges(*tf, range);
            cout << "Found " << in_range.size() 
                 << " stale block pairs exceeding "
                 << range << " days:" << endl;
            for (auto r=in_range.begin(); r!=in_range.end(); ++r) {
                auto first = (*r).first;
                auto second = (*r).second;
                auto days = Block::getRangeDifference(first, second);
                cout << "==> " << (*tf).getName()
                     << ": Stale Range (" << days << " Days)"
                     << " (Lines " << first->getFirstLineNum() << " to " 
                     << second->getFirstLineNum() << ") "
                     << "(Blocks " << first->_id << ", " << second->_id << ")"
                     << endl;
            }
        }
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
