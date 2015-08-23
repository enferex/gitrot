## gitrot: Locate stale comments associated to code blocks.

### What
gitrot is a utility that locates comments that might no longer be
relevant to the code that they are associated with.  The idea is pretty simple.
Comments follow code.  During the development cycle, code will get
updated/fixed; however, comments do not always get corrected along with the
code.  This can create the case where a comment is no longer correct or
relevant.  Semantic parsing of comments is non-trivial.  At best this tool will
try to locate comments that are much older than the following block of code
(where old is user specified).  It is up to the user to determine if the
comments are incorrect.

### Dependencies
git: https://git-scm.org

### Building
Run *make* from the source directory.  The resulting binary can be copied
anywhere.

### Contact
mattdavis9@gmail.com (enferex)
