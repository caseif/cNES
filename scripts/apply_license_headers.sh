#!/bin/bash

YEAR="2019"
AUTHOR="Max Roncace"
EMAIL="mproncace@gmail.com"

# read the header template
header=$(cat HEADER.txt)

# apply replacements
header=$(sed -r "s/\\$\{year\}/$YEAR/" <<< "$header")
header=$(sed -r "s/\\$\{author\}/$AUTHOR/" <<< "$header")
header=$(sed -r "s/\\$\{email\}/$EMAIL/" <<< "$header")

# add an asterisk to the start of each line
header=$(sed -r "s/^/ * /" <<< "$header")

# add /* */ to the start and end
header="/*\n$header\n */"\

# discover all source files
find "./src" -type f -iname "*.c" -print0 | while IFS= read -r -d $'\0' file; do
    #TODO: figure out how to detect if files are up-to-date

    if grep -Pzoq "(?s)\/\*.*Copyright \(c\).*?\*\/" "$file"; then
        echo "Updating header for file $file"

        tmp="$file.tmp"

        # adapted from https://stackoverflow.com/a/21702566
        # this basically replaces the license pattern with our current formatted license,
        # with a neat trick to deal with a multi-line context
        gawk -v RS='^$' -v hdr="$header\n\n" '{sub(/\/\*.*?Copyright \(c\).*?\*\/\n*/,hdr)}1 {printf $0}' $file > "$tmp" && mv $tmp $file
    else
        echo "Generating new header for file $file"
        
        # save output to intermediate variable with trailing character
        # this prevents bash from stripping trailing newlines
        output="$header\n\n$(cat $file && echo .)"
        
        # remove the last character and write the output back to the file
        printf "${output:0:-1}" > $file
    fi
done
