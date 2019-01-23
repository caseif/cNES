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

# discover all source/header files
find "./include" "./src" -type f \( -iname "*.c" -or -iname "*.h" \) -print0 | while IFS= read -r -d $'\0' file; do
    #TODO: figure out how to detect if files are up-to-date

    if grep -Pzoq "(?s)\/\*.*Copyright \(c\).*?\*\/" "$file"; then
        echo "Updating header for file $file"

        # adapted from https://stackoverflow.com/a/21702566
        # this basically replaces the license pattern with our current formatted license,
        # with a neat trick to deal with a multi-line context
        # we also append a dot to preserve trailing whitespace during substitution
        output=$(gawk -v RS='^$' -v hdr="$header\n\n" '{sub(/\/\*.*?Copyright \(c\).*?\*\/\n*/,hdr)}1 {print $0}' $file && echo .)

        # escape any formatting sequences present in the code since printf likes to convert them
        output="${output//%/%%}"
        # escape previously-escaped newlines since printf likes to convert them too
        output="${output//\\n/\\\\n}"
        # print appends a newline, so we need to remove it and the trailing dot we added
        output="${output:0:-2}"
        # finally, write it to disk!
        printf "$output" > $file
    else
        echo "Generating new header for file $file"
        
        # save output to intermediate variable with trailing character
        # this prevents bash from stripping trailing newlines
        output="$header\n\n$(cat $file && echo .)"
        
        # remove the last character and write the output back to the file
        printf "${output:0:-1}" > $file
    fi
done
