genc_dir=$(dirname $0)
if [[ -z "${USE_LDRGEN}" ]]; then
	gcc "${genc_dir}/generate_c.c" -o "${genc_dir}/generate_c"
	${genc_dir}/generate_c $@ > /tmp/quadra_genc.c
else
	# HACK: Keep generating functions until we get one with the right prototype.
	src=""
	while [[ ${src} != *"int fn1(int p)"* ]]; do
		src=$(frama-c -ldrgen -ldrgen-int-only -ldrgen-max-args 1 -ldrgen-no-div-mod -ldrgen-no-pointer-args)
	done
	echo -e \
		"#pragma GCC diagnostic ignored \"-Woverflow\"\n" \
		"${src}\n" \
		"#ifdef __x86_64__\n" \
		"int main() {\n" \
		"#else\n" \
		"int _start() {\n" \
		"#endif\n" \
		"return fn1(0);\n" \
		"}\n" \
		> /tmp/quadra_genc.c
fi
gcc /tmp/quadra_genc.c -o /tmp/quadra_genc_native
/tmp/quadra_genc_native
target_code=$(echo $?)
ee-gcc /tmp/quadra_genc.c -o /tmp/quadra_genc_mips -nostdlib -w -std=c99 -O3
${genc_dir}/../quadra /tmp/quadra_genc_mips > /tmp/quadra_genc_llvm.ll
clang /tmp/quadra_genc_llvm.ll "${genc_dir}/../libmips_o32_linux.a" -o /tmp/quadra_genc_translated
/tmp/quadra_genc_translated
exit_code=$?
echo "exit code: ${exit_code}, target code: ${target_code}"
if [ "$exit_code" == "$target_code" ]; then
	exit 0
else
	exit 1
fi
