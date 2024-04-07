#!/bin/bash

project_dir=$(realpath ../../)
io_dir=$(realpath "$project_dir"/src/io)
output_dir=output
suffix=cminus

LOG=log.txt

usage() {
	cat <<JIANMU
Usage: $0 [path-to-testcases] [type]
path-to-testcases: './testcases' or '../../testcases_general' or 'self made cases'
type: 'debug' or 'test', debug will output .ll file
JIANMU
	exit 0
}

check_return_value() {
	rv=$1
	expected_rv=$2
	fail_msg=$3
	detail=$4
	if [ "$rv" -eq "$expected_rv" ]; then
		return 0
	else
		printf "\033[1;31m%s: \033[0m%s\n" "$fail_msg" "$detail"
		return 1
	fi
}

# check arguments
[ $# -lt 2 ] && usage
if [ "$2" == "debug" ]; then
	debug_mode=true
elif [ "$2" == "test" ]; then
	debug_mode=false
else
	usage
fi

test_dir=$1
testcases=$(ls "$test_dir"/*."$suffix" | sort -V)
check_return_value $? 0 "PATH" "unable to access to '$test_dir'" || exit 1

# hide stderr in the script
# exec 2>/dev/null

mkdir -p $output_dir

truncate -s 0 $LOG

if [ $debug_mode = false ]; then
	exec 3>/dev/null 4>&1 5>&2 1>&3 2>&3
else
	exec 3>&1
fi

if [ $debug_mode = false ]; then
	exec 1>&4 2>&5
fi

regval=reg
gccval=gcc

echo "[info] Start testing, using testcase dir: $test_dir"
printf "\033[33;1mtestcase \t\t \033[32;1mafter optimization\033[0m \t\t \033[35;1mbaseline\033[0m\n"
# asm
for case in $testcases; do
	echo "==========$case==========" >>$LOG
	case_base_name=$(basename -s .$suffix "$case")
	std_out_file=$test_dir/$case_base_name.out
	in_file=$test_dir/$case_base_name.in
	asm_file=$output_dir/$case_base_name.s
	exe_file=$output_dir/$case_base_name
	out_file=$output_dir/$case_base_name.out
	ll_file=$output_dir/$case_base_name.ll

	asm_file_reg="$output_dir/${case_base_name}_${regval}.s"
	exe_file_reg="$output_dir/${case_base_name}_${regval}"
	out_file_reg="$output_dir/${case_base_name}_${regval}.out"
	ll_file_reg="$output_dir/${case_base_name}_${regval}.ll"

	exe_file_gcc="$output_dir/${case_base_name}_${gccval}"
	out_file_gcc="$output_dir/${case_base_name}_${gccval}.out"

	# if debug mode on, generate .ll also
	if [ $debug_mode = true ]; then
		bash -c "cminusfc -mem2reg -emit-llvm $case -o $ll_file_reg" >>$LOG 2>&1
	fi

    # 不进行寄存器分配
	bash -c "cminusfc -S -mem2reg $case -o $asm_file" >>$LOG 2>&1
	check_return_value $? 0 "CE" "cminusfc compiler error" || continue

	loongarch64-unknown-linux-gnu-gcc -static \
		"$asm_file" "$io_dir"/io.c -o "$exe_file" \
		>>$LOG
	check_return_value $? 0 "CE" "gcc compiler error" || continue


	# 进行寄存器分配
	bash -c "cminusfc -S -mem2reg -register-allocate $case -o $asm_file_reg" >>$LOG 2>&1
	check_return_value $? 0 "CE" "cminusfc compiler error" || continue

	loongarch64-unknown-linux-gnu-gcc -static \
		"$asm_file_reg" "$io_dir"/io.c -o "$exe_file_reg" \
		>>$LOG
	check_return_value $? 0 "CE" "gcc compiler error" || continue

    #gcc编译，进行比较
    case_gcc_name=$test_dir/$case_base_name.c
    bash -c "cp $case $case_gcc_name"
	loongarch64-unknown-linux-gnu-gcc -O0 -w -static \
		"$case_gcc_name" "$io_dir"/io.c -o "$exe_file_gcc" \
		>>$LOG
	check_return_value $? 0 "CE" "gcc compiler error" || continue

    elapsed_time_reg=0
    elapsed_time_gcc=0
    compare_time=1
    i=1

    while(($i<=$compare_time))
    do
        start_time=$(date +%s%N)
        # qemu run
        if [ -e "$in_file" ]; then
            exec_cmd="qemu-loongarch64 $exe_file >$out_file <$in_file"
        else
            exec_cmd="qemu-loongarch64 $exe_file >$out_file"
        fi
        time (bash -c "$exec_cmd") > time.txt 2>&1
        ret=$?
        end_time=$(date +%s%N)
        run_time=$(( (end_time - start_time) / 1000000 ))
        # remove trailing null byte in the end line
        sed -i "\$s/\x00*$//" "$out_file"
        # append return value
        echo $ret >>"$out_file"
        let elapsed_time=run_time+elapsed_time
        let "i++"      
    done

    i=1
    while(($i<=$compare_time))
    do
        start_time=$(date +%s%N)
        # qemu run
        if [ -e "$in_file" ]; then
            exec_cmd="qemu-loongarch64 $exe_file_reg >$out_file_reg <$in_file"
        else
            exec_cmd="qemu-loongarch64 $exe_file_reg >$out_file_reg"
        fi
        time (bash -c "$exec_cmd") > time.txt 2>&1
        ret=$?
        end_time=$(date +%s%N) 
        run_time=$(( (end_time - start_time) / 1000000 ))
        # remove trailing null byte in the end line
        sed -i "\$s/\x00*$//" "$out_file_reg"
        # append return value
        echo $ret >>"$out_file_reg"
        let elapsed_time_reg=run_time+elapsed_time_reg
        let "i++" 
    done

    i=1
    while(($i<=$compare_time))
    do
        start_time=$(date +%s%N)
        # qemu run
        if [ -e "$in_file" ]; then
            exec_cmd="qemu-loongarch64 $exe_file_gcc >$out_file_gcc <$in_file"
        else
            exec_cmd="qemu-loongarch64 $exe_file_gcc >$out_file_gcc"
        fi
        time (bash -c "$exec_cmd") > time.txt 2>&1
        ret=$?
        end_time=$(date +%s%N) 
        run_time=$(( (end_time - start_time) / 1000000 ))
        # remove trailing null byte in the end line
        sed -i "\$s/\x00*$//" "$out_file_gcc"
        # append return value
        echo $ret >>"$out_file_gcc"
        let elapsed_time_gcc=run_time+elapsed_time_gcc
        let "i++" 
    done

    # elapsed_time_avg=$(echo "scale=5; $elapsed_time / ($compare_time) " | bc)
    # elapsed_time_avg_format=$(printf "%.2f" "$elapsed_time_avg")
    elapsed_time_reg_avg=$(echo "scale=5; $elapsed_time_reg / ($compare_time) " | bc)
    elapsed_time_reg_avg_format=$(printf "%.2f" "$elapsed_time_reg_avg")
    elapsed_time_gcc_avg=$(echo "scale=5; $elapsed_time_gcc / ($compare_time) " | bc)
    elapsed_time_gcc_avg_format=$(printf "%.2f" "$elapsed_time_gcc_avg")
    printf "%-20s \t       %-4.2f \t\t\t   %-4.2f\n" $case_base_name.cminus $elapsed_time_reg_avg_format $elapsed_time_gcc_avg_format
    # printf "%-4.2f\n" $elapsed_time_avg_format
    rm $case_gcc_name

# time ./your_program
done
rm time.txt