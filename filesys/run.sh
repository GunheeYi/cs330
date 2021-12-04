make
cd build
clear
rm -f tmp.dsk
rm -f tmp.dsk
pintos-mkdisk tmp.dsk 2
pintos -v -k -T 60 -m 20   --fs-disk=tmp.dsk -p tests/filesys/extended/dir-open:dir-open -p tests/filesys/extended/tar:tar --swap-disk=4 -- -q   -f run dir-open
pintos -v -k -T 120   --fs-disk=tmp.dsk -g fs.tar:tests/filesys/extended/dir-open.tar --swap-disk=4 -- -q  run 'tar fs.tar /'