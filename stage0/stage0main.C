#include <stage0.h>
int main(int argc, char **argv)
{
// This program is the stage0 compiler for Pascallite. It will accept
// input from argv[1], generate a listing to argv[2], and write object
// code to argv[3].
if (argc != 4) // Check to see if pgm was invoked correctly
{
// No; print error msg and terminate program
cerr << "Usage: " << argv[0] << " SourceFileName ListingFileName "
<< "ObjectFileName" << endl;
exit(EXIT_FAILURE);
}
Compiler myCompiler(argv);
myCompiler.createListingHeader();
myCompiler.parser();
myCompiler.createListingTrailer();
return 0;
}
Compiler(char **argv) // constructor
{
open sourceFile using argv[1]
open listingFile using argv[2]
open objectFile using argv[3]
}
~Compiler() // destructor
{
close all open files
}
void createListingHeader()
{
print "STAGE0:", name(s), DATE, TIME OF DAY
print "LINE NO:", "SOURCE STATEMENT"
//line numbers and source statements should be aligned under the headings
}
void parser()
{
nextChar()
//ch must be initialized to the first character of the source file
if (nextToken() != "program")
processError(keyword "program" expected)
//a call to nextToken() has two effects
// (1) the variable, token, is assigned the value of the next token
// (2) the next token is read from the source file in order to make
// the assignment. The value returned by nextToken() is also
// the next token.
prog()
//parser implements the grammar rules, calling first rule
}
void createListingTrailer()
{
print "COMPILATION TERMINATED", "# ERRORS ENCOUNTERED"
}
void processError(string err)
{
Output err to listingFile
Call exit(EXIT_FAILURE) to terminate program
}
