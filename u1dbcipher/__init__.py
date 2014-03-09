# -*- coding: utf-8 -*-
# __init__.py
# Copyright (C) 2014 LEAP Leap Encryption Access Project
#
# This program is free software: you can redistribute it and/or modify
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
u1dbcipher initialization.
Python bindings for libu1db with SQLCipher support
"""
from __future__ import print_function

__version_info__ = (13, 10)
__version__ = '.'.join(map(lambda x: '%02d' % x, __version_info__))

try:
    from u1dbcipher import _u1db
    assert(_u1db)
except ImportError as exc:
    print("Cannot import _u1db extension: %s" % exc.message)
    _u1db = None


if _u1db is not None:
    def open(*args):
        """
        :param sqlcipher_file: The path for the SQLCipher file.
        :type sqlcipher_file: str

        :param password: The password that protects the SQLCipher db.
        :type password: str

        :param raw_key: Whether `password` is a raw 64-char hex string or a
                        passphrase that should be hashed to obtain the
                        encyrption key.
                        (Unused in the c api at the moment)
        :type raw_key: bool

        :param cipher: The cipher and mode to use.
        :type cipher: str

        :param kdf_iter: The number of iterations to use.
        :type kdf_iter: int

        :param cipher_page_size: The page size.
        :type cipher_page_size: int
        """
        # TODO
        # should pass also:
        # [ ] ISOLATION LEVEL
        # [ ] CHECK_SOME_THREAD
        # [ ] PRAGMA_SYNCHRONOUS
        # [ ] WAL MODE
        return _u1db.CSQLCipherDatabase(*args)
