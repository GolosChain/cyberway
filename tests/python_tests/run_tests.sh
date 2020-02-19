#!/bin/bash

dir=$(dirname $0)

status=0

function execute_test {
    test_name=$1
    test_command=$2
    echo "Starting test: $test_name ($test_command)"

    python3 $dir/$test_command #> /dev/null 2>&1

    if [[ $? -eq 0 ]]; then
        echo -e "\e[1;32mPassed\e[0m"
    else
        echo -e "\e[1;31mFailed\e[0m"
        status=1
    fi
}

