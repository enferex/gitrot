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

class Block;

class Line
{
public:
    // Parsing utils
    static string getTextLine(FILE *fp);
    static Line *getLine(FILE *fp);

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
    string _contents;

    friend ostream &operator<<(ostream &os, const Line &tf) {
        return os << tf._contents;
    }
};

typedef vector<Line *> Lines;
typedef Lines::iterator LinesItr;

class Block
{
public:
    const Lines &getLines(void) const { return _lines; }
    void addLine(Line *line) { _lines.push_back(line); }

private:
    Lines _lines;
};
typedef vector<Block *> Blocks;

class CommentBlock : public Block
{
};

class CodeBlock : public Block
{
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
             os << "==> Block "<< cnt++ << ": ("
                << lines.size() << " lines)" << endl;
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
    ln->_contents         = getTextLine(fp);

    return ln;
}

Block *TranslationFile::createBlock(LinesItr &itr)
{
    return NULL;
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

#ifdef DEBUG
    for (auto i=lines.begin(); i!=lines.end(); ++i)
      cout << **i;
#endif

    // File has been read in, now put the lines into blocks
    for (auto i=lines.begin(); i!=lines.end(); ++i) {
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
