cd libffi
./autogen.sh
./configure --prefix=`pwd`/output
make
make install
cd ..

cd text2cpp
./autogen.sh
./configure --prefix=`pwd`/output
make
make install
cd ..
