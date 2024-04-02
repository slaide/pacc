function run_test () {
    argument="$1"
    command="bin/main $argument"

    $command

    if [ $? -eq 0 ]; then
        echo "Success: '$command'"
    else
        echo "Error: '$command' failed with exit code $?"
        exit $?
    fi
}

for f in $(ls test/*.c)
do
run_test $f
done