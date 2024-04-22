function run_test () {
    printf "\033[1mRunning test: '$1'\033[0m\\n"
    argument="$1"
    command="bin/main $argument"

    $command

    if [ $? -eq 0 ]; then
        printf "\033[1;32mSuccess: '$command'\033[0m\\n"
    else
        printf "\033[1;31mError: '$command' failed with exit code $?\033[0m, test for : "
        grep "^// TESTGOAL" $argument | sed 's/\/\/ TESTGOAL //'
        exit $?
    fi
}

for f in $(ls test/t*.c)
do
run_test $f
done