Getting started
===============

The easiest way to get started is to use "setup.py install", preferably
in a virtualenv (although that's up to you).


Using the database interface
============================

Inserting and retrieving documents::

    import u1db
    db = u1db.open(":memory:", create=True)
    doc = db.create_doc({"firstname": "Bob", "familyname": "Foo"})
    print "document id: %s" % doc.doc_id
    print "document revision: %s" % doc.revision

    doc2 = db.get_doc(doc.doc_id)
    print (doc == doc2)

Creating an index::

    import u1db
    db = u1db.open(":memory:", create=True)
    # Indexes can be on a single field...
    idx = db.create_index("firstname_idx", ["firstname"])
    # ... or on several
    idx2 = db_create_index("composed_idx", ["firstname", "familyname"])

Querying using an index::

    import u1db
    db = u1db.open(":memory:", create=True)
    doc = db.get_from_index("firstname_idx", [("Bob",)])
    print doc
    # You can of course get a range of documents using wildcards
    docs = db.get_from_index("firstname_idx", [("Bo*",)])

More information and comments can be found in the u1db/__init__.py file.


Running the tests
=================

A simple `make check` should get you on the right tracks. The dependencies can
be infered from the output.


Building the docs
=================

cmake . && make html-docs

after which the reference implementation documentation can be found at::

html-docs/_build/html/index.html

and the C implementation documentation at::

html-docs/_doxygen/html/index.html

though a more useful starting point may be::

_doxygen/html/u1db_8h.html
