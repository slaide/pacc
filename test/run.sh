function run_test () {
    printf "\033[1mRunning test: '$1'\033[0m\\n"
    argument="$1"
    command="bin/main $argument"

    $command

    if [ $? -eq 0 ]; then
        printf "\033[1;32mSuccess: '$command'\033[0m\\n"
    else
        printf "\033[1;31mError: '$command' failed with exit code $?\033[0m"
        # if this does not match a line, print a newline characters instead
        grep "^// TESTFOR " $argument | sed 's/\/\/ TESTFOR /, test for : /'
        printf "\\n"
        exit $?
    fi
}

for f in $(ls test/t*.c)
do
run_test $f
done