#!/bin/bash
##############################################################
## Title: pandoc-convert
## Abstact: Converts from pandoc markdown to various formats
## Author: Fabian Raab <fabian.raab@tum.de>
## Dependencies: bash, pandoc, sed, mktemp
##      Files in the same dir as script: markdown.template
##############################################################

SCRIPTNAME=$(basename $0)
SCRIPTPATH="$0"
EXIT_SUCCESS=0
EXIT_FAILURE=1
EXIT_ERROR=2
EXIT_BUG=10

SRC_DIR=$(dirname $0)
DST_DIR=$(dirname $0)
 
# count return codes
rc=$EXIT_SUCCESS

cd $SRC_DIR

for file in "$SRC_DIR"/*.markdown; do

echo "Converting $file"

filetitle=$(basename --suffix=".markdown" "$file"; rc=$(($rc + $?)))

# extract NUM of the first occurrence of '(NUM)'. The script assumes that in
# the first line of $file the name and section of the man page is defined:
#      % CAN-ETH-GW(1) ....
section=$(grep --max-count=1 -Po '^.*?\K(?<=\().*?(?=\))' "$file"; \
	rc=$(($rc + $?)))

temp_with_table=$(mktemp --suffix=".markdown")
# removes spaces/tabs before '|' at beginning of line. Converts a table as code
# block to a markdown table
sed 's%^[\t ]*|\(.*\)$%|\1%g' ${file} > $temp_with_table
	rc=$(($rc + $?))

temp_with_table_greater=$(mktemp --suffix=".markdown")
# replace ':' with '>' at beginning of lines
sed 's%^[ ]*\:\(.*\)$%> \1%g' $temp_with_table > $temp_with_table_greater
	rc=$(($rc + $?))

## Man Page
pandoc -s -t man -i $temp_with_table -o ${DST_DIR}/${filetitle}.${section}
	rc=$(($rc + $?))

## Standart Github Flavoured Markdown
pandoc -t markdown_github --template=${SRC_DIR}/markdown.template \
	-i $temp_with_table_greater \
	-o ${DST_DIR}/${filetitle}.${section}.md
	rc=$(($rc + $?))

# pandoc removes \n before a '>' and add a escape. This command withdraw it.
sed -i 's%\\>%\n>%g' ${DST_DIR}/${filetitle}.${section}.md
	rc=$(($rc + $?))

## Strict Markdown
pandoc -t markdown_strict --template=${SRC_DIR}/markdown.template \
	-i $temp_with_table_greater \
	-o ${DST_DIR}/${filetitle}.${section}.strict.md
	rc=$(($rc + $?))

# pandoc removes \n before a '>' and add a escape. This command withdraw it.
sed -i 's%\\>%\n>%g' ${DST_DIR}/${filetitle}.${section}.strict.md
	rc=$(($rc + $?))

## Strict Markdown with Jeky II header
pandoc -t markdown_strict --template=${SRC_DIR}/jekyii.template \
	-i $temp_with_table_greater \
	-o ${DST_DIR}/${filetitle}.${section}.jekyii.md
	rc=$(($rc + $?))

# pandoc removes \n before a '>' and add a escape. This command withdraw it.
sed -i 's%\\>%\n>%g' ${DST_DIR}/${filetitle}.${section}.jekyii.md
	rc=$(($rc + $?))

## HTML
pandoc -s -t html --template=${SRC_DIR}/html.template \
	-i $temp_with_table \
	-o ${DST_DIR}/${filetitle}.${section}.html
	rc=$(($rc + $?))

done

exit $rc