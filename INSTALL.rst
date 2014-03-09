Build patched u1db library
==========================
cython u1dbcipher/_u1db.pyc
cmake .
make u1db

Build c extension
======================
python setup.py build

Install python package
======================
python setup.py install
