#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cerrno>
#include <cstdint>
#include <cstring>
using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::string;
using std::vector;

class Line
{
public:
    Line(const string str) {_line = str;};
private:
    string   _line;
    string   _user;
    string   _date;
    uint64_t _revision;
};

typedef vector<Line> Lines;

class Block
{
private:
    vector<Line> _Lines;
};
typedef vector<Block> Blocks;

class CommentBlock
{
};

class CodeBlock
{
};

class Thing
{
    public:
    Thing() { cout << "Thing" << endl; }
};

class TranslationFile
{
public:
    TranslationFile(const char *fname);

private:
    void parse(ifstream &fs);
    string _fname;
    Blocks _blocks;
    Thing t;
};

void TranslationFile::parse(ifstream &fs)
{
    system(("git blame " + _fname).c_str());
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

    return 0;
}
