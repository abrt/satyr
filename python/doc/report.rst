.. _report:
.. currentmodule:: satyr

Report
======

A :class:`Report` object represents a single problem report in the `uReport format <https://github.com/abrt/faf/wiki/uReport>`_.

.. autoclass:: Report
   :members:

Operating system
----------------

Data about operating system are stored in the :class:`OperatingSystem` object.

.. autoclass:: OperatingSystem
   :members:

Package
-------

The :class:`Report` can contain a list of software packages. Currently, only `RPM <http://www.rpm.org/>`_ packages are supported.

.. autoclass:: RpmPackage
   :members:
