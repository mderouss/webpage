# Build a Webpage from markdown
---

A 'C' program to combine body text with head metadata to make a *simple* web page.
Really, brutally simple. That's the idea. 

Usage :

webpage [-h] [-v] [-f \<flags\> ] [-c \<abs path to html root\>] [-n \<navembedcode\>] \<markdown file\>

The generated HTML will be put in a file named after the provided .md file. 

-h prints help text.

-v specifies verbose output on stderr. 

-f If the corresponding flag is set to 1 in \<flags\>, then that item will be omitted.

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

---

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
