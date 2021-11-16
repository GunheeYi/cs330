make
cd build
pintos -v -k -T 180 -m 8   --fs-disk=10 -p tests/vm/swap-iter:swap-iter -p ../../tests/vm/large.txt:large.txt --swap-disk=10 -- -q   -f run swap-iter