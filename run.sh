cd threads
make clean
make
cd ..
source ./activate
cd threads/build
clear
# make check VERBOSE=1
pintos -- -q run alarm-multiple
cd ../..