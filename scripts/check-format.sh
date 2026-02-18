#!/bin/bash
set -eu

readonly scriptpath=$0;
readonly scriptdir=$(dirname $0)
readonly srcdir=$scriptdir/../xb
readonly testdir=$scriptdir/../xb

function main(){
    format_files $srcdir
    format_files $testdir
}

function format_file(){
    local inpfile=$1; shift
    local tmpfile=$(mktemp)
    clang-format --style=file $inpfile > $tmpfile
    if [[ $(diff -q $inpfile $tmpfile) ]]
    then
        echo "reformatting $inpfile"
        mv $tmpfile $inpfile
    fi

    rm -f $tmpfile
}

function format_files(){
    local inpdir=$1; shift
    local source_files=$(find $inpdir -type f -name "*.hpp")
    for source_file in $source_files
    do
        if [[ -f "$source_file" ]]
        then
            format_file $source_file
        fi
    done
}


main
