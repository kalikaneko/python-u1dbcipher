python-u1dbcipher
=================

Python bindings for `libu1db`_, with support for `SQLCipher`_.

This is done for the `Soledad`_ (*Synchronization Of Locally Encrypted Data Among Devices*) component of the `LEAP`_ project.

A previous attempt at this same goal is `pysqlcipher`_, but the python
u1db implementation is severely limited in performance when it comes to sync
several hundreds of documents.

Right now this is an experimental package with a couple of trivial modifications
to the original u1db code:

* extending a little bit the `cython`_ bindings that can be found in the test
  folder from the ubuntu u1db reference implementation written in python
* patching libu1db to initialize SQLCipher with the needed crypto pragmas.

.. _`libu1db`: https://launchpad.net/u1db
.. _`SQLCipher`: http://sqlcipher.net/
.. _`Soledad`: https://github.com/leapcode/soledad
.. _`LEAP`: https://leap.se
.. _`pysqlcipher`: https://github.com/leapcode/pysqlcipher 
.. _`cython`: https://cython.org 


Install
=======

Build patched u1db library::

    cython u1dbcipher/_u1db.pyc
    cmake .
    make u1db

Build c extension::

    python setup.py build

Install python package::

    python setup.py install

Using the database interface
============================

Inserting and retrieving documents
-----------------------------------

Note that you have to give an extra argument
to the ``open`` function for the encryption key::

    import u1dbcipher as u1db
    db = u1db.open("/tmp/test.db", "sekret")
    doc = db.create_doc_from_json("{'theanswer': 42}")
    print "document id: %s" % doc.doc_id
    print "document revision: %s" % doc.rev

    doc2 = db.get_doc(doc.doc_id)
    print (doc == doc2)

You can visually check that the database file is encrypted::

    hexdump -C /tmp/test.db | head                                                                

    00000000  00 04 6b b3 3a 7f af 4e  1b 37 b7 c2 40 90 18 37  |..k.:..N.7..@..7|
    00000010  2a 4d 8c e3 1b cd 60 7c  cf 84 c1 57 ef cb 54 91  |*M....`|...W..T.|
    00000020  5a 48 27 0d 36 42 85 90  82 a8 a4 20 3d ec 2f e4  |ZH'.6B..... =./.|
    00000030  92 ac 64 f3 d4 c5 fb c7  96 ef 84 21 91 61 a8 f1  |..d........!.a..|
    00000040  23 70 d3 00 3b fd f3 f7  29 1f 8d 86 e5 9a 03 e9  |#p..;...).......|
    00000050  23 8e 85 60 59 c5 f2 e5  b4 75 60 ec 4c a1 bc 5c  |#..`Y....u`.L..\|
    00000060  dc 93 6f 84 ed 78 34 4b  bb 79 2d bd 67 d5 32 82  |..o..x4K.y-.g.2.|
    00000070  eb 4f ec b3 75 07 fb 52  16 b6 0f 27 0a e2 89 4c  |.O..u..R...'...L|
    00000080  35 6d c0 00 23 63 3b 96  83 83 6f e1 19 fd 0f 44  |5m..#c;...o....D|
    00000090  b1 69 82 ab 12 de 5d 4a  60 2a 9d f2 31 58 60 b3  |.i....]J`*..1X`.|

