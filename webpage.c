/*
Build a Webpage from markdown
=============================

A program to combine body text with head metadata to make a *simple* web page.
Really, brutally simple. That's the idea. 

Usage :

webpage [-h] [-v] [-f <flags> ] [-c <abs path to html root>] [-n navembedcode] <markdown file>

The generated HTML will be put in a file named after the provided .md file. 

-h prints help text.

-v specifies verbose output on stderr. 

-f If the corresponding flag is set to 1 in <flags>, then that item will be omitted.

    0x01 DOCTYPE
    0x02 Title
    0x04 Datetime
    0x08 Author

-c specifies that CSS should be included, and provides the root of the directory
   hierarchy for searching purposes. CSS is included using the algorithm described 
   below.

-n adds HTML provided via this option to the end of the body. My purpose for this 
   is for adding an embedding to use as a navigation mechanism, but it's effectively
   general purpose. 

===

First, the program adds a DOCTYPE declaration, but this can be omitted. This is
almost certainly a mistake, but you can do it.

Then, the program makes the head.

It adds a 'title' tag derived from the .md file name by default, but this can 
be omitted. This too might be a mistake, but you can do it.

It adds a datetime, but this can be omitted.

It adds an author ( username of the user running the program ), but this can be 
omitted. 

If permitted on the command line, it adds a *link* to a css file. Otherwise, *no 
style tag is added and there will be no style info in the html*, but see below.
The css file to be linked is found by searching up the directory hierarchy towards
the root given. The first css file found is used. 

If a .txt file is provided in the current directory with the same name as the 
.md file, it includes the contents of that file directly in the head, after any 
css link. No format conversion is done, so this file should be valid html. So 
if you really need to add a shedload of metadata or extend your default css, 
you can do it this way. 

Then it adds the body.

The program operates on a .md file containing the body text. It assumes that
the .md file is in markdown format corresponding to the CommonMark standard. 

It uses the cmark library to convert md files to html. To be precise, it uses the
library in UNSAFE mode, because this allows raw html to be specified in the md
files, which is the only way the Commonmark designers allow basic shit like 
strikethroughs that you've been able to do safely with a typewriter for 130 years.  

================================================================================

Licenced using MIT licence : 

Copyright 2021 Mark de Roussier

Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to use, 
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
Software, and to permit persons to whom the Software is furnished to do so, subject 
to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

// Standard headers
#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
// Need __USE_GNU for canonicalize_file_name() from stdlib
#define __USE_GNU  
#include <stdlib.h>
#include <pwd.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
// libcmark
#include <cmark.h>

/****************************** Local types ***************************************/

/**
 * Bitwise flags for turning off default tags 
 */
enum flagValues
{
    flag_dtd        = 0x01,
    flag_title      = 0x02,
    flag_datetime   = 0x04,
    flag_author     = 0x08,
    flag_end        = 0x0
};

/**
 * Values of command line Options 
 */
struct Options
{
    bool    includeTitle;
    bool    includeDTD;
    bool    includeAuthor;
    bool    includeDatetime;
    char    *cssRoot;
    char    *navEmbedCode;
    char    *txtFilename;
    bool    verbose;
};

/****************************** Global variables **********************************/

/**
 * Filenames
 */

char *g_MarkdownFilename    = NULL;
char *g_RootFilename        = NULL;
char *g_WebpageFilename     = NULL;

/**
 * Web page ( html ) output file pointer
 */
FILE *g_WebpageFilePtr  = NULL;

/**
 * Command line options set to default values
 */
struct Options g_Options = { true, true, true, true, NULL, NULL, NULL, false };

/***** Constants *****/

/**
 * The initial opening string for a web page, according to HTML5. We are NOT SUPPORTING
 * HTML versions less than 5.
 */
const char *g_Doctype = "<!DOCTYPE html>\n";

/**
 * Tag texts. Other tags expected to come from markdown conversion. 
 */
const char *g_PageOpenTag       = "<html>\n";
const char *g_PageCloseTag      = "</html>\n";
const char *g_HeadOpenTag       = "<head>\n";
const char *g_HeadCloseTag      = "</head>\n";
const char *g_TitleOpenTag      = "<title>";
const char *g_TitleCloseTag     = "</title>\n";
const char *g_CommentOpenTag    = "<!--";
const char *g_CommentCloseTag   = "-->\n";
const char *g_LinkOpenTag       = "<link ";
const char *g_LinkCloseTag      = " >\n";
const char *g_BodyOpenTag       = "<body>\n";
const char *g_BodyCloseTag      = "</body>\n";

/**
 * Exit codes
 * 
 * Exit strategy : exit on user error, message on stdout.
 * 
 */
const int  EXIT_NORMAL                      = 0;
const int  EXIT_NO_MARKDOWN                 = -1;
const int  EXIT_MISSING_VALUE               = -2;
const int  EXIT_UNKNOWN_OPTION              = -3;
const int  EXIT_NO_CSS_FILE                 = -4;
const int  EXIT_ABS_PATH_REQUIRED           = -5;
const int  EXIT_INVOKED_OUTSIDE_HTML_ROOT   = -6;

/**
 * Arbitrary value for max size of relative path to css file. 
 * Note that this will contain only '../' elements, plus a css filename.
 * It's considered highly unlikely that there will be more than a handful
 * of parent directories in practice, though in the event of user error 
 * we may end up traversing all the way back up to root. 20 levels still
 * gives us 194 characters for a css filename.
 */
const int  SEARCH_DIR_SIZE          = 256;

/************************** Declarations ******************************************/

extern void   verbose( char *outputSpecifier, ... );
extern void   copyFile( FILE *inputPtr, FILE *outputPtr );
extern void   checked_fwrite( const char *stringToWritePtr );
extern char   *getUserName( void );
extern void   includeCSSFile( void );
extern void   includeTxtFile( void );
extern void   addWebpageHead( void );
extern void   addWebpageBody( void );
extern void   makeWebpageFile( void );
extern void   makeWebpage( void );
extern void   getOptions( int argc, char **argv );

/****************************** Code **********************************************/

/**
 * void verbose( char *outputSpecifier, ... )
 * 
 * Print output on stderr if the verbose option has been specified. 
 * 
 * in   : outputSpecifier   -   a format string
 * in   : ...               -   varargs for format string
 * out  : format string written to stderr, if verbose flag true
 * err  : none
 */
void verbose( char *outputSpecifier, ... )
{
    if( g_Options.verbose )
    {
    va_list args;

        va_start( args, outputSpecifier ); 

        vfprintf( stderr, outputSpecifier, args );
    }
}

/**
 * void copyFile( FILE *inputPtr, FILE *outputPtr )
 * 
 * Copy the whole of the input file to the output file in a moderately efficient way
 * using the block size as provided by fstat. 
 * 
 * in       : inputPtr  - FILE pointer to the input file
 * in       : outputPtr - FILE pointer to the output file
 * out      : The input file is copied into the output file
 * err      : assert if we fail to allocate a read buffer. 
 * err      : assert if we fail to write exactly as much as we have read. 
 */
void copyFile( FILE *inputPtr, FILE *outputPtr )
{
struct stat inputStat;
int         inputFD;
char        *readBufferPtr = NULL;
int         bytesRead = 0;
int         written = 0;

    inputFD = fileno( inputPtr );

    fstat( inputFD, &inputStat );

    readBufferPtr = (char *)malloc( inputStat.st_blksize );

    assert( readBufferPtr );

    while( inputStat.st_size >= ( bytesRead += fread( readBufferPtr, sizeof(char), inputStat.st_blksize, inputPtr ) ) )
    {
        if( bytesRead < inputStat.st_size )
        {
            // there's data remaining to be read, so we've read a block, so write the complete block
            written += fwrite(readBufferPtr, inputStat.st_blksize, 1, outputPtr );

            verbose( "We've read %d and written %d and the size is %d \n", bytesRead, written, inputStat.st_blksize );
        }
        else
        {
            // up to now we've been counting writes in blocks, but now we're going to write chars,
            // so change 'written' so we can honestly keep track. NB If the size of the file is smaller than
            // a block, written will be zero, so this still works.

            written = written * inputStat.st_blksize;

            // write the remainder - bytesRead%inputStat.st_blksize
            // We lose a little efficiency here if the file size is an exact multiple of the blocksize
            written += fwrite( readBufferPtr, sizeof(char), bytesRead%inputStat.st_blksize, outputPtr );

            verbose( "We've read %d and written %d and the size is %d \n", bytesRead, written, inputStat.st_blksize );

            // we're done
            break;
        }
    }

    assert( bytesRead == written );
}

/**
 * void checked_fwrite( char *stringToWritePtr, FILE *fileToWritePtr )
 * 
 * Wrapper to check that fwrite has written successfully.
 * 
 * in   : stringToWritePtr  -   points to the NULL terminated string to be written
 * out  : The supplied string has been written to the supplied file
 * err  : assert if failed to write expected number of characters.
 */
void checked_fwrite( const char *stringToWritePtr )
{
int written = 0;

    written = fwrite( stringToWritePtr, sizeof(char), strlen(stringToWritePtr), g_WebpageFilePtr );

    verbose("Wrote %d chars to file, tried %d chars\n", written, strlen(stringToWritePtr) );

    assert( written == strlen(stringToWritePtr) );
}

/**
 * char *getUserName( void )
 * 
 * Get the username of the effective user. NB - the struct passwd is not
 * intended to be freed by the caller. 
 * 
 * in   :   none
 * out  :   username corresponding to the euid ( effective user id )
 * err  :   assert if the euid cannot be mapped to a user
 */
char *getUserName( void )
{
unsigned int    uid = geteuid();
struct passwd   *pw = getpwuid(uid);

    verbose( "Attempt to get username...\n" );

    assert( pw );

    verbose( "...gives %s\n", pw->pw_name );

    return( strdup( pw->pw_name ) );
}

/**
 * char *findCssFile()
 * 
 * Discover the css file to be linked to, if any.
 * 
 * We must know the absolute path of the html root, because we're not 
 * going to look beyond that for css files. 
 * 
 * So, assume that it is this path that is given via the 'c' option ( 
 * if there's no 'c' option, we're not looking for css files ). 
 * 
 * We can't handle a relative path, because there's no clear way to 
 * interpret it - at the point we are invoked, we don't know how deeply 
 * nested we are.
 * 
 * Then, we need a way to discover successive parent directories and search
 * them for css. NB - we do *not* need to, and should not, change the 
 * current working directory. But, we need to know the absolute path of 
 * every directory that we traverse so that we can test it against the
 * root. 
 * 
 * Just to make life interesting, the string we want to place in the 
 * html link ( i.e. the return value ) *is* relative to the location of 
 * the html file. 
 * 
 * in   :   none
 * out  :   relative path to css file, else NULL if no css found
 * err  :   assert if failed to open directory
 * err  :   assert if failed to canonicalize filename
 * err  :   exit if invoked outside of specified html root.  
 */
char *findCssFile( void )
{
DIR             *dirPtr;
struct dirent   *contentPtr;
char            searchDir[SEARCH_DIR_SIZE + 1];
char            *absPathPtr = NULL;
bool            result=false;

    // searchDir is equivalent to the relative part of the result,
    // and is sized to allow addition of the css filename when we 
    // find one. 

    strcpy( searchDir, "./" );

    do
    {
        // is there a css file in the current directory ?

        verbose( "Searching %s for css...\n", searchDir );

        dirPtr = opendir( searchDir );
        
        assert( NULL != dirPtr );

        while( contentPtr = readdir( dirPtr ) )
        {
            if( NULL != strstr( contentPtr->d_name, ".css" ) )
            {
                // we found a css file - return the relative path

                strcat( searchDir, contentPtr->d_name );

                verbose( "Found css, file is %s\n", searchDir );

                // yes, yes, it's naughty, so sue me...

                return( strdup(searchDir) );
            }
        }

        // was searchdir the html root ?

        absPathPtr = canonicalize_file_name( searchDir );

        assert( absPathPtr );

        verbose( "Real path just searched was %s\n", absPathPtr );

        if( 0 == strcmp( "/", absPathPtr ) )
        {
            // we were not under root when we started ! 

            printf( "Invoked outside of html root hierarchy.\n" );
            exit( EXIT_INVOKED_OUTSIDE_HTML_ROOT );
        }

        result = ( 0 != strcmp( absPathPtr, g_Options.cssRoot ) );

        strcat( searchDir, "../" );

        free( absPathPtr );
    } 
    while ( result );

    verbose( "Found no css file under %s\n", g_Options.cssRoot );

    return( NULL );
}

/** 
 * void includeCSSFile( FILE *webpageFilePtr )
 * 
 * Search for a css file, and include a link to it.
 * 
 * in   : none
 * out  : A link field pointing at the css file is written to the html
 * err  : exit if we do not in fact have a css file
 */
void includeCSSFile( void )
{
char*   cssFilenamePtr;

    cssFilenamePtr = findCssFile();

    if( NULL == cssFilenamePtr )
    {
        printf( "No css file found under %s\n", g_Options.cssRoot );
        exit( EXIT_NO_CSS_FILE );
    }

    verbose( "Writing link to css file %s\n", cssFilenamePtr );

    checked_fwrite( g_LinkOpenTag );
    checked_fwrite( " rel=\"stylesheet\" href=\"" );
    checked_fwrite( cssFilenamePtr );
    checked_fwrite( "\" " );
    checked_fwrite( g_LinkCloseTag );
}       

/**
 * void includeTxtFile( FILE *webpageFilePtr )
 * 
 * First, see if we've got a txt file with an appropriate name. If we have, copy 
 * it into the web page header verbatim.
 * 
 * in       :   none
 * out      :   Text from txt file is in the header.
 * err      :   assert if cannot obtain memory for text file name.
 */
void includeTxtFile( void )
{
FILE *txtFilePtr = NULL;
int  txtFilenameSize = strlen(g_RootFilename)+strlen(".txt") + 1;
char *txtFilenamePtr = (char *)calloc( txtFilenameSize, sizeof(char) );

    assert( txtFilenamePtr );

    strcat( txtFilenamePtr, g_RootFilename );
    strcat( txtFilenamePtr, ".txt" );

    verbose( "Opening txt file %s\n", txtFilenamePtr );

    txtFilePtr = fopen( txtFilenamePtr, "r" );

    if( NULL == txtFilePtr )
    {
        verbose( "Txt file %s not provided\n", txtFilenamePtr );
    }
    else
    {
        verbose( "Copying %s into web page\n", txtFilenamePtr );
        copyFile( txtFilePtr, g_WebpageFilePtr );
    }
}       

/**
 * void addWebpageBody( FILE *htmlFile )
 * 
 * Process the markdown file, and add the rendered html to the
 * web page file. 
 * 
 * in       : none
 * out      : html body is written to html file.
 * err      : assert on NULL mdFile file pointer
 * err      : assert on failure to parse file ( NULL node tree ptr )
 * err      : assert on failure to render ( NULL render buffer ptr )
 */
void addWebpageBody( void )
{
cmark_node* nodeTreePtr = NULL;
char*       renderBufferPtr = NULL;
int         htmlSize = 0;
int         writtenSize = 0;
FILE*       mdFilePtr;

    mdFilePtr = fopen( g_MarkdownFilename, "r" );

    assert( mdFilePtr );

    verbose("Opened markdown file %s for read only\n", g_MarkdownFilename );

    checked_fwrite( g_BodyOpenTag );

    verbose( "Parsing markdown file\n" );

    nodeTreePtr = cmark_parse_file( mdFilePtr, CMARK_OPT_UNSAFE );

    assert( nodeTreePtr );

    verbose( "Rendering HTML\n" );

    renderBufferPtr = cmark_render_html( nodeTreePtr, CMARK_OPT_UNSAFE );

    // We own the nodeTreePtr and must free it
    cmark_node_free( nodeTreePtr );

    assert( renderBufferPtr );

    checked_fwrite( renderBufferPtr );

    // We own the render buffer, and must free it.
    free( renderBufferPtr );

    // Before closing the body, add the navigation embedding, if provided 

    if( NULL != g_Options.navEmbedCode )
    {
        verbose( "Add navigation embedding %s - FINAL FORM TBD !!! \n", g_Options.navEmbedCode );
        checked_fwrite( g_CommentOpenTag );
        checked_fwrite( " NAVIGATION EMBEDDING GOES HERE \n" );
        checked_fwrite( g_Options.navEmbedCode );
        checked_fwrite( g_CommentCloseTag );
    }

    checked_fwrite( g_BodyCloseTag );

    fflush( g_WebpageFilePtr );
}

/**
 * addWebpageHead( FILE *webpageFilePtr )
 * 
 * Add head tags. Between those tags add any permitted Options,
 * including CSS and raw content.   
 * 
 * in   :   none
 * out  :   Web page file contains a complete head.
 * err  :   assert if time string buffer is wrongly sized.
 */
void addWebpageHead( void )
{
int written = 0;

    checked_fwrite( g_HeadOpenTag );

    if( g_Options.includeTitle )
    {
        // Title is the root part of the md file

        verbose( "Writing Title as %s\n", g_RootFilename );

        checked_fwrite( g_TitleOpenTag );
        checked_fwrite( g_RootFilename );
        checked_fwrite( g_TitleCloseTag );
    }

    if( g_Options.includeAuthor )
    {
    char *usernamePtr = NULL;

        usernamePtr = getUserName();

        checked_fwrite( g_CommentOpenTag );
        checked_fwrite( "Author is " );
        checked_fwrite( usernamePtr );
        checked_fwrite( g_CommentCloseTag );

        free( usernamePtr );
    }

    if( g_Options.includeDatetime )
    {
    time_t      t   = time( NULL );
    struct tm   *tm  = localtime( &t );
    char        s[64];

        verbose( "Time is %ld \n", t );

        assert( strftime(s, sizeof(s), "%c", tm) );

        checked_fwrite( g_CommentOpenTag );
        checked_fwrite( "Datetime is " );
        checked_fwrite(  s );
        checked_fwrite( g_CommentCloseTag );        
    }

    if( NULL != g_Options.cssRoot )
    {
        includeCSSFile();
    }

    // Include a txt file, if it's there

    includeTxtFile();

    checked_fwrite( g_HeadCloseTag );

    fflush( g_WebpageFilePtr );
}

/**
 * FILE *makeWebpageFile( void )
 * 
 * Assume the current working directory is where the html file is required, and 
 * create it. 
 * 
 * in       : none
 * out      : Web page file handle. The web page file has been created.
 * err      : assert if failed to allocate web page file name 
 * err      : assert if failed to open web page file for w
 */
void makeWebpageFile( void )
{
FILE *webpageFilePtr = NULL;

    // Make a file with the name of the .md file but with a .html extension instead.

    g_WebpageFilename = (char *)calloc( sizeof(char), strlen( g_RootFilename )+strlen(".html")+1 );

    assert( g_WebpageFilename );

    strcpy( g_WebpageFilename, g_RootFilename );
    strcat( g_WebpageFilename, ".html" );

    verbose("Using web page filename of %s\n", g_WebpageFilename );

    g_WebpageFilePtr = fopen( g_WebpageFilename, "w" );

    assert( g_WebpageFilePtr );
}

/**
 * void makeWebpage( void )
 * 
 * Create the actual html file corresponding to the markdown file, and 
 * add the required head and body information to it. 
 * 
 * in       : none
 * out      : Web page file has been written.
 * err      : none
 */
void makeWebpage( void )
{
FILE *webpageFilePtr = NULL;
FILE *mdFilePtr = NULL;
int  written = 0;

    // Create output file

    makeWebpageFile();

    // Add the HTML DOCTYPE - this comes before the head ! 
    // NB Assume HTML 5 ! Means no specific DTD.
    // If for some bonkers reason you don't want this, you can omit it

    if( g_Options.includeDTD )
    {
        checked_fwrite( g_Doctype );
    }

    // but you can't omit this...

    checked_fwrite( g_PageOpenTag );

    // Add header info

    addWebpageHead();

    // Add body info

    addWebpageBody();

    // Add the page closing tag

    checked_fwrite( g_PageCloseTag );

    fflush( g_WebpageFilePtr );

    fclose( g_WebpageFilePtr );
}

/**
 * void printHelp( void )
 * 
 * Print help info.
 * 
 * in   :   none
 * out  :   help info on stdout
 * err  :   none
 */
void printHelp( void )
{
    printf( "Usage: webpage [-h] [-v] [-f <flags> ] [-c <abs path to html root>] [-n navembedcode] <markdown file>\n\n" );

    printf( "Broadly speaking, the expectation is that this tool will be used as part of a script that\n" );
    printf( "traverses a directory hierarchy containing md files and converts them to html. In particular,\n" );
    printf( "it's expected that the markdown file resides in the current working directory, and that this\n" );
    printf( "directory is underneath html root ( yes, html root not markdown root ).\n\n"); 

    printf( " -h                        : print this help\n" );
    printf( " -v                        : output verbose information\n" );
    printf( " -f <flags>                : <flags> are bitwise as follows -\n" );
    printf( "                           : 0x01 - omit DOCTYPE \n" );
    printf( "                           : 0x02 - omit title \n" );
    printf( "                           : 0x04 - omit datetime\n" );
    printf( "                           : 0x08 - omit author\n" );
    printf( " -c <abs path to html root>: Enables linking to css file. File linked will be first found\n" );
    printf( "                             searching from cwd towards html root. NB Requires an absolute,\n" );
    printf( "                             not relative, path.\n" );
    printf( " -n <navembedcode>         : Provide for ability to tack a special embedding on to the body for\n" );
    printf( "                             navigation purposes.\n" );
    printf( " <markdown file>           : File containing Commonmark markdown. Beware of complications if\n" );
    printf( "                             this file is not in the cwd, particularly if using the 'c' option.\n\n");
}

/**
 * void getOptions( int argc, char **argv )
 * 
 * Work out what the user supplied parameters are.  
 * 
 * in       : argc  -   count of command line arguments
 * in       : argv  -   array of command line arguments
 * out      : g_Options values set if specified on command line
 * err      : exit if help requested.
 * err      : exit if c path not absolute.
 * err      : exit if missing option value.
 * err      : exit if unknown option specified. 
 * err      : assert on failure to parse command line.
 * err      : exit if no markdown filename provided.
 * err      : assert on excess arguments.
 */
void getOptions( int argc, char **argv )
{
int     option = 0;
char    *extensionPtr = NULL;

    // we will handle errors explicitly
    opterr = 0;

    // first, just see if we are verbose because we want to 
    // be verbose about option processing too...

    while( -1 != ( option = getopt(argc, argv, "v") ) )
    {
        switch(option)
        {
            case 'v' :  
            {
                g_Options.verbose = true;
                verbose( "Verbose reporting ON\n" );
                break;
            } 
            // Ignore everything else
        }
    }

    // reset getopt internal state
    optind = 0;

    // extract values of options

    while( -1 != ( option = getopt(argc, argv, "hvf:c:n:") ) )
    {
        verbose( "Processing option %c\n", option );
        switch(option)
        {
            case 'h' :
            {
                printHelp();
                exit( EXIT_NORMAL );
            }
            case 'v' :  
            {
                // This option already handled if present
                break;
            }
            case 'f' :  
            {
                long flags = 0;

                verbose( "Read Flags as %s\n", optarg );

                flags = strtol( optarg, NULL, 16 );

                verbose( "Converted Flags as %ld\n", flags );

                if( flag_dtd & flags )g_Options.includeDTD = false;
                verbose( "dtd is %d\n",g_Options.includeDTD );
                if( flag_title & flags )g_Options.includeTitle = false;
                verbose( "title is %d\n",g_Options.includeTitle );
                if( flag_author & flags )g_Options.includeAuthor = false;
                verbose( "author is %d\n",g_Options.includeAuthor );
                if( flag_datetime & flags )g_Options.includeDatetime = false;
                verbose( "datetime is %d\n",g_Options.includeDatetime );

                break;
            }
            case 'c' : 
            {
                verbose( "Read css root as %s\n", optarg );

                /********************************************/
                /* NB - the cssRoot is an absolute path     */
                /* to the root of a hierarchy that will be  */
                /* searched for .css files.                 */
                /********************************************/

                if( '/' != optarg[0] )
                {
                    printf("Absolute path required for 'c' option\n");
                    exit( EXIT_ABS_PATH_REQUIRED );
                }

                g_Options.cssRoot = strdup( optarg );
                break;
            }
            case 'n' :
            {
                verbose( "Read navigation embedding as %s\n", optarg );
                g_Options.navEmbedCode = strdup( optarg );
                break;
            }
            case ':' :  
            {
                printf( "Option %d requires a value\n", optopt ); 
                exit(EXIT_MISSING_VALUE);
                break;
            }
            case '?' :  
            {
                printf( "Unknown option %d specified\n", optopt );
                exit(EXIT_UNKNOWN_OPTION);
                break;
            }
            default  :  
            {
                verbose( "Bad option %d\n", option ); 
                assert( false );
            }
        }
    }

    // We are expecting a markdown filename to be supplied as the last
    // command line arg.

    verbose( "optind is %d, argc is %d\n", optind, argc );

    if ( optind == argc )
    {
        printf("Expecting a markdown file to be specified\n");
        exit(EXIT_NO_MARKDOWN);
    }

    // There should only be one naked argument on the command line
    assert( (optind + 1) == argc );

    verbose( "Using %s as markdown filename\n", argv[optind] );

    g_MarkdownFilename = strdup( argv[optind] );

    // Extract the root of the md filename ( i.e. filename without extension )

    g_RootFilename = strdup( g_MarkdownFilename );
    
    // Use the part of the md filename before any extension. Don't assume there's a .md extension.
    extensionPtr = strrchr(g_RootFilename, '.');

    if( NULL != extensionPtr )
    {
        *extensionPtr = '\0';
    }
 
    verbose( "Root filename is %s\n", g_RootFilename );
}

/**
 * void main( int argc, char** argv )
 * 
 * Get user options and make webpage.
 * 
 */
void main( int argc, char** argv )
{
    // Get g_Options, and g_MarkdownFilename, and g_RootFilename

    getOptions( argc, argv );

    // Assemble web page

    makeWebpage();

    exit(EXIT_NORMAL);
}
