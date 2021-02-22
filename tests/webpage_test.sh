#! /bin/bash

# Test runner for webpage program.
#
# This script runs webpage in a variety of ways, and compares 
# it's output to reference material. If you have updated webpage
# such that it generates different output to that contained in 
# markdown_test_reference directory, the tests will fail. It's
# up to you to determine whether this is a problem with webpage,
# or a problem with the reference material.
#
# Run this script from webpage/tests
#
# See webpage_test_manifest.txt for individual test description.
#

################# test prep ######################

# check result of the webpage run. 
# NB - filter differences that are just down to the Datetime
# string, if that's present. 
function checkResult {
result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: $1 webpage returned ${result}"
    exit -1
fi

diff -I '.*Datetime.*' $1.html ../markdown_test_reference/$1.html >$1.diff
result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: $1 html diff failure"
    exit -1
fi

echo "webpage_test.sh: $1 success"

}

# Record datetime of test and code
dt=$(date '+%Y:%m:%d-%H:%M:%S');
echo "webpage_test.sh: webpage test run ${dt}"
program=$(which webpage)
programdate=$(date -r ${program})
echo "webpage_test.sh: using ${program} modified ${programdate}"

# copy the test markdown to html root
# if html root already exists, the previous run probably failed,
# so preserve it. 

if [[ -d markdown_test_html ]]
then
    echo "webpage_test.sh: markdown_test_html should not exist, preserving..."
    mv markdown_test_html ${dt}_failed_test
fi

cp -r markdown_test markdown_test_html

# tests must run from html root

cd markdown_test_html
rm *.backup

################## run tests #####################

# 1
echo "webpage_test.sh: Running webpage_test1"
webpage webpage_test1.md

checkResult webpage_test1

# 2
echo "webpage_test.sh: Running webpage_test2"
webpage webpage_test2.md

checkResult webpage_test2

# 3
echo "webpage_test.sh: Running webpage_test3"
webpage webpage_test3.md

checkResult webpage_test3

# 4
echo "webpage_test.sh: Running webpage_test4"
webpage webpage_test4.md

checkResult webpage_test4

# 5
echo "webpage_test.sh: Running webpage_test5"
webpage -f 0x05 webpage_test5.md

checkResult webpage_test5

# 6
echo "webpage_test.sh: Running webpage_test6"
webpage -f 0x0c webpage_test6.md

checkResult webpage_test6

# 7
echo "webpage_test.sh: Running webpage_test7"
webpage -f 0x06 webpage_test7.md

checkResult webpage_test7

# 8
echo "webpage_test.sh: Running webpage_test8"
webpage -f 0x04 webpage_test8.md

checkResult webpage_test8

# 9
# Parameter value for 'c' must be abs path to html root
abshtmlroot=${PWD}
echo "webpage_test.sh: Running webpage_test9"
webpage -c ${abshtmlroot} webpage_test9.md

checkResult webpage_test9

# 10
echo "webpage_test.sh: Running webpage_test10"
webpage -n "Navigation Embedding" webpage_test10.md

checkResult webpage_test10

# 11
echo "webpage_test.sh: Running webpage_test11"
webpage -v webpage_test11.md 2>webpage_test11_verbose.txt
result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: webpage_test11 webpage returned ${result}"
    exit -1
fi

diff -I '.*Datetime.*' webpage_test11.html ../markdown_test_reference/webpage_test11.html >webpage_test11.diff
result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: webpage_test11 html diff failure"
    exit -1
fi

diff -I '.*Time is.*' webpage_test11_verbose.txt ../markdown_test_reference/webpage_test11_verbose.txt >webpage_test11_verbose.diff
result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: webpage_test11 verbose diff failure"
    exit -1
fi

echo "webpage_test.sh: webpage_test11 success"

# 12
echo "webpage_test.sh: Running webpage_test12"
webpage -h > webpage_test12_help.txt 
result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: webpage_test12 webpage returned ${result}"
    exit -1
fi

diff webpage_test12_help.txt ../markdown_test_reference/webpage_test12_help.txt >webpage_test12_help.diff
result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: webpage_test12 help diff failure"
    exit -1
fi

echo "webpage_test.sh: webpage_test12 success"

#13a
echo "webpage_test.sh: Running webpage_test13a"
cd webpage_test13
cd webpage_test13a
webpage -c ${abshtmlroot} webpage_test13a.md

result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: webpage_test13a webpage returned ${result}"
    exit -1
fi

diff -I '.*Datetime.*' webpage_test13a.html ../../../markdown_test_reference/webpage_test13/webpage_test13a/webpage_test13a.html >webpage_test13a.diff
result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: webpage_test13a html diff failure"
    exit -1
fi

echo "webpage_test.sh: webpage_test13a success"
cd ../..

#13b
echo "webpage_test.sh: Running webpage_test13b"
cd webpage_test13
cd webpage_test13b
webpage -c ${abshtmlroot} webpage_test13b.md

result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: webpage_test13b webpage returned ${result}"
    exit -1
fi

diff -I '.*Datetime.*' webpage_test13b.html ../../../markdown_test_reference/webpage_test13/webpage_test13b/webpage_test13b.html >webpage_test13b.diff
result=$?
if [[ ${result} -ne 0 ]]
then
    echo "webpage_test.sh: webpage_test13b html diff failure"
    exit -1
fi

echo "webpage_test.sh: webpage_test13b success"
cd ../..

################### Preserve the successful test #####################

cd ..
mv markdown_test_html ${dt}_markdown_test_html





