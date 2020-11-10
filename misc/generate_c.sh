misc_dir=$(dirname $0)
gcc "${misc_dir}/generate_c.c" -o "${misc_dir}/generate_c"
${misc_dir}/generate_c $@ > /tmp/quadra_genc.c
gcc /tmp/quadra_genc.c -o /tmp/quadra_genc_native
/tmp/quadra_genc_native
target_code=$(echo $?)
ee-gcc /tmp/quadra_genc.c -o /tmp/quadra_genc_mips -nostdlib -w -std=c99 -O3
${misc_dir}/../quadra /tmp/quadra_genc_mips > /tmp/quadra_genc_llvm.ll
clang /tmp/quadra_genc_llvm.ll -o /tmp/quadra_genc_translated
/tmp/quadra_genc_translated
exit_code=$?
echo "exit code: ${exit_code}, target code: ${target_code}"
if [ "$exit_code" == "$target_code" ]; then
	exit 0
else
	exit 1
fi
