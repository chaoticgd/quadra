pass=0
fail=0
for ((i = 0; i < 100; i++)); do
	$(dirname $0)/generate_c.sh $RANDOM
	if [[ $? -ne 0 ]]
	then
		fail=$((fail+1))
	else
		pass=$((pass+1))
	fi
done

echo "pass ${pass}, fail ${fail}"
