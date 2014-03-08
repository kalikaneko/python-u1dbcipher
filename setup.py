# -*- coding: utf-8 -*-
#!/usr/bin/env python
# setup.py.py
# Copyright 2011 Canonical Ltd.
# Copyright (C) 2014 LEAP Encryption Access Project
#
# python-cu1db is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""
python-cu1db setup script
"""

from __future__ import print_function
import sys


def config():
    try:
        from setuptools import setup, Extension, find_packages
    except ImportError:
        from distutils.core import setup, Extension, find_packages  # noqa
    import cu1db
    kwargs = {}
    try:
        from Cython.Distutils import build_ext
    except ImportError:
        print("Unable to import Cython! Install it and try again please.")
        sys.exit(1)
    else:
        kwargs["cmdclass"] = {"build_ext": build_ext}
        extra_libs = []
        extra_defines = []
        if sys.platform == 'win32':
            # Used for the random number generator
            extra_libs.append('advapi32')
            extra_libs.append('libcurl_imp')
            extra_libs.append('libeay32')
            extra_defines = [('_CRT_SECURE_NO_WARNINGS', 1)]
        else:
            extra_libs.append('curl')
        extra_libs.append('json')
        ext = [(Extension(
            "cu1db._u1db",
            ["cu1db/_u1db.pyx"],
            include_dirs=['/usr/include'],
            library_dirs=["/usr/lib"],
            # XXX should link directly against libsqlcipher in leap case.
            # XXX add some switch to the build process?!
            libraries=['u1db', 'sqlite3', 'oauth'] + extra_libs,
            define_macros=[] + extra_defines,
            ))]
    kwargs.update({
        "name": "python-cu1db",
        "version": cu1db.__version__,
        "description": "Simple syncable document storage",
        "url": "https://launchpad.net/u1db",
        "license": "GNU LGPL v3",
        "author": "Ubuntu One team",
        "author_email": "u1db-discuss@lists.launchpad.net",
        "maintainer": 'Kali Kaneko',
        "maintainer_email": 'kali@leap.se',
        #"download_url": "https://launchpad.net/u1db/+download",
        "packages": find_packages(),
        "package_data": {'': ["*.sql"]},
        #"scripts": ['u1db-client', 'u1db-serve'],
        "ext_modules": ext,
        # XXX dirspect needs external flag
        "install_requires": ["dirspec", "paste", "routes", "oauth"],
        # informational
        "tests_require": ["Cython", "pyOpenSSL"],
        "classifiers": [
            'Development Status :: 4 - Beta',
            'Environment :: Console',
            'Intended Audience :: Developers',
            'License :: OSI Approved :: '
            'GNU Library or Lesser General Public License (LGPL)',
            'Operating System :: OS Independent',
            'Operating System :: Microsoft :: Windows',
            'Operating System :: POSIX',
            'Programming Language :: Python',
            'Programming Language :: Cython',
            'Topic :: Software Development :: Debuggers',
        ],
        "long_description": """\
A simple syncable JSON document store.

This allows you to get, retrieve, index, and update JSON documents, and
synchronize them with other stores.
"""
    })
    setup(**kwargs)

if __name__ == "__main__":
    config()
