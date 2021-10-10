make
cd build
pintos -v -k -T 60 -m 20 --fs-disk=10 -p tests/userprog/dup2/dup2-complex:dup2-complex -p ../../tests/userprog/dup2/sample.txt:sample.txt -- -q   -f run dup2-complex