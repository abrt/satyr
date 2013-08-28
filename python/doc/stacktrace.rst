.. _stacktrace:
.. currentmodule:: satyr

Stacktrace
==========

Each problem has its own frame, stacktrace and thread type (for problems that
can have multiple threads).

.. warning::
   The :class:`GdbFrame`, :class:`GdbThread`, and :class:`GdbStacktrace` have
   rather limited functionality, cannot be used in reports, and will probably
   be removed in the future.

Frame classes
-------------

Stack frame corresponds to a function invocation saved on the stack. All frame
types are derived from the :class:`BaseFrame`.

BaseFrame
^^^^^^^^^

.. autoclass:: BaseFrame
   :members:

CoreFrame
^^^^^^^^^

.. autoclass:: CoreFrame
   :members:

JavaFrame
^^^^^^^^^

.. autoclass:: JavaFrame
   :members:

KerneloopsFrame
^^^^^^^^^^^^^^^

.. autoclass:: KerneloopsFrame
   :members:

PythonFrame
^^^^^^^^^^^

.. autoclass:: PythonFrame
   :members:

GdbFrame
^^^^^^^^

.. autoclass:: GdbFrame
   :members:

Thread classes
--------------

Thread classes are defined only for problems that can have multiple thread.
They all have :class:`BaseThread` as a base class.

BaseThread
^^^^^^^^^^

.. autoclass:: BaseThread
   :members:

CoreThread
^^^^^^^^^^

.. autoclass:: CoreThread
   :members:

JavaThread
^^^^^^^^^^

.. autoclass:: JavaThread
   :members:

GdbThread
^^^^^^^^^^

.. autoclass:: GdbThread
   :members:

Stacktrace classes
------------------

Single threaded stacktraces have :class:`SingleThreadStacktrace` as their base
class, which is in turn derived from :class:`BaseThread`. This means that
:class:`SingleThreadStacktrace` can be treated as a thread:

 * :class:`Kerneloops`
 * :class:`PythonStacktrace`

Stacktrace types with the possibility of multiple threads are derived from
:class:`MultiThreadStacktrace`:

 * :class:`CoreStacktrace`
 * :class:`JavaStacktrace`
 * :class:`GdbStacktrace`

SingleThreadStacktrace
^^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: SingleThreadStacktrace
   :members:

MultiThreadStacktrace
^^^^^^^^^^^^^^^^^^^^^

.. autoclass:: MultiThreadStacktrace
   :members:

CoreStacktrace
^^^^^^^^^^^^^^

.. autoclass:: CoreStacktrace
   :members:

JavaStacktrace
^^^^^^^^^^^^^^

.. autoclass:: JavaStacktrace
   :members:

Kerneloops
^^^^^^^^^^

.. autoclass:: Kerneloops
   :members:

PythonStacktrace
^^^^^^^^^^^^^^^^

.. autoclass:: PythonStacktrace
   :members:

GdbStacktrace
^^^^^^^^^^^^^

.. autoclass:: GdbStacktrace
   :members:
