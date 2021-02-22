#!/bin/bash

<<'###BLOCK-COMMENT'
MIT Licence :
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
###BLOCK-COMMENT

# This is a script that aims to take a website written in markdown, contained in a 
# single directory hierarchy ( i.e. a set of directories with a common root ), and
# build a copy of the directory hierarchy that contains html files in place of md
# files ( plus any other files the site needs from the markdown hierarchy ).
# 
# The html files are generated from the md files using webpage, a utility which relies
# on the Commonmark rendering engine, i.e there's an assumption that the flavour of
# markdown you are using is Commonmark compatible.  
# 
# webpage provides a mechanism for linking to css files, and if this is to be 
# done then this script must provide the relevant option, including an absolute 
# path to the root of the html hierarchy ( which it works out for itself ). 
#
# The navigation embed code is a special feature. If provided, it attaches the 
# provided embed code to the end of the html ( last thing before the </body> tag ).
# I have a use for this, you may not :). 
#
# Links within md files to other md files are converted into links to html files,
# because the assumption is that you've developed the whole site in markdown using
# something like ghostwriter, and those files may be cross linked. 
#
# Some tidy up is done to remove unwanted files from the html directory tree. 
#
# opt n is a navigation embed code ( defaults to none )
# opt m is the root of the markdown tree ( default is . )
# opt h is the root of the HTML tree ( default is ../${PWD##*/}_html )
# opt c is to turn on linking of css files ( default is false ) 
# opt F is flags options - specify special behaviors to webpage program, see 
#                          webpage -h .
# opt v is to turn on verbose output ( default is false ).
#
# NB - do not confuse options as supplied to this script with the options this
# script provides to the webpage utility. They are related, but not identical.
#

# Variables with names ending in 'Option' are arguments to the webpage program.
# Since all such options are optional ( ahem ), they start out empty.

navembedcode=""
markdownroot="."
htmlroot="../${PWD##*/}_html"
css=false
verbose=false
flags=""

navembedcodeOption=""
cssOption=""
verboseOption=""
flagsOption=""

while getopts ":cvn:m:h:F:" opt; do
  case ${opt} in
    n )
      navembedcode=${OPTARG}
      ;;
    m )
      markdownroot=${OPTARG}
      ;;
    h )
      htmlroot=${OPTARG}
      ;;
    c )
      css=true
      ;;
    v )
      verbose=true
      ;;
    F )
      flags=${OPTARG}
      ;;
    \? )
      echo "Invalid option: ${OPTARG}" 1>&2
      exit
      ;;
    : )
      echo "Invalid option: ${OPTARG} requires an argument" 1>&2
      exit
      ;;
  esac
done
shift $((OPTIND -1))

echo "Using Navigation Embed Code : ${navembedcode} "
echo "Using Markdown root         : ${markdownroot} "
echo "Using HTML root             : ${htmlroot} "
echo "Using CSS option            : ${css} "
echo "Using verbose option        : ${verbose} "
echo "Using Flags                 : ${flags} "

while true; do
    echo ===================================================================
    read -p "Do you wish to continue?" yn
    case $yn in
        [Yy]* ) break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done

# Interpret the option settings

if [[ ${verbose} == true ]]
then
    verboseOption=" -v "
fi

if [[ ${navembedcode} != "" ]]
then
    navembedcodeOption=" -n ${navembedcode} "
fi

if [[ ${flags} != "" ]]
then
    flagsOption=" -F ${flags} "
fi

# Make sure we're located *in markdown root*

cd ${markdownroot}
absmarkdownroot=${PWD}
echo "Absolute markdown root is ${absmarkdownroot}" 

# Create a new html root. 

if [[ -d ${htmlroot} ]] 
then 
# Backup html root

    dt=$(date '+%Y:%m:%d-%H:%M:%S');
    echo "Backing up HTML root to ${htmlroot}.${dt} "
    mv ${htmlroot} ${htmlroot}.${dt}
else
    echo "HTML root not found"
fi

# Should now be no html root, so create new one

if [ ! -d ${htmlroot} ]
then
    echo "Making HTML root"
    mkdir ${htmlroot} 
else
    echo "Unexpected HTML root found, aborting"
    exit -1
fi

# Recursively copy the content of markdown root ( we're in the markdown root ) to 
# html root. Here, we grab everything. Later on, we try to tidy up. 

cp -r * ${htmlroot}

# Switch to the html root

cd ${htmlroot}
abshtmlroot=${PWD}
echo "Absolute html root is ${abshtmlroot}" 

# if the user has asked for css, set up the option and add the abs html root

if [[ ${css} ]]
then
    cssOption=" -c ${abshtmlroot} "
fi

# Make HTML with my webpage tool. 
# 
# Note that when webpage executes, it's directory context is the context of the md file. 
# This is very important. If the 'c' option is given, it will find css files by searching
# upwards in the directory hierarchy. It will use the first css file that it encounters. 
# By putting different css files at different levels of the hierarchy, a site default css 
# can be overridden in a flexible manner. If the 'c' option is not provided, no css search 
# will be performed.
# 
# However, the css search needs an end condition - this is provided by assuming that the 
# search starts somewhere beneath html root, and will stop when it gets to html root. An
# unambiguous ( absolute ) value of html root must therefore be provided. 
#
# The webpage tool implements another customisation technique - if a file of the same name
# as the .md file but with a .txt extension is present in the directory, it is loaded 
# into the html file 'as is' in the head section. 
#
# NB webpage uses the cmark html renderer. 

find . -name "*.md" -execdir webpage ${verboseOption} ${flagsOption} ${cssOption} ${navembedcodeOption} '{}' \; 

# Recursively replace references to .md files in .html files to references to .html
# files. This situation exists because building the whole site in markdown may mean
# having links that point to markdown files, not html files. 
#
# This replacement is dangerous if the web page is talking about .md files :). Try to 
# limit the potential damage by using '">' for context. It would really be nicer to do 
# this in the renderer, when we would have more context. Of course, it's very likely 
# there's a smarter way to use sed regex's. I'm thinking about that. 

find . -name "*.html" -execdir sed -i 's/\.md\">/\.html\">/g' '{}' \;

# Tidy up - try to leave the HTML tree in a state where it can just be copied to 
# the web server. Be careful about deleting files from the HTML tree, because the 
# website needs files other than just HTML files ( e.g. images ), and it's hard to
# be confident what they are ahead of time. 

find . -name "*.md" -delete
find . -name "*.backup" -delete
find . -name "*.txt" -delete

<<'###BLOCK-COMMENT'
###BLOCK-COMMENT







