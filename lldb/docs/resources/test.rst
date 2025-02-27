Testing
=======

.. contents::
   :local:

Test Suite Structure
--------------------

The LLDB test suite consists of three different kinds of test:

* **Unit tests**: written in C++ using the googletest unit testing library.
* **Shell tests**: Integration tests that test the debugger through the command
  line. These tests interact with the debugger either through the command line
  driver or through ``lldb-test`` which is a tool that exposes the internal
  data structures in an easy-to-parse way for testing. Most people will know
  these as *lit tests* in LLVM, although lit is the test driver and ShellTest
  is the test format that uses ``RUN:`` lines. `FileCheck
  <https://llvm.org/docs/CommandGuide/FileCheck.html>`_ is used to verify
  the output.
* **API tests**: Integration tests that interact with the debugger through the
  SB API. These are written in Python and use LLDB's ``dotest.py`` testing
  framework on top of Python's `unittest2
  <https://docs.python.org/2/library/unittest.html>`_.

All three test suites use ``lit`` (`LLVM Integrated Tester
<https://llvm.org/docs/CommandGuide/lit.html>`_ ) as the test driver. The test
suites can be run as a whole or separately.


Unit Tests
``````````

Unit tests are located under ``lldb/unittests``. If it's possible to test
something in isolation or as a single unit, you should make it a unit test.

Often you need instances of the core objects such as a debugger, target or
process, in order to test something meaningful. We already have a handful of
tests that have the necessary boiler plate, but this is something we could
abstract away and make it more user friendly.

Shell Tests
```````````

Shell tests are located under ``lldb/test/Shell``. These tests are generally
built around checking the output of ``lldb`` (the command line driver) or
``lldb-test`` using ``FileCheck``. Shell tests are generally small and fast to
write because they require little boilerplate.

``lldb-test`` is a relatively new addition to the test suite. It was the first
tool that was added that is designed for testing. Since then it has been
continuously extended with new subcommands, improving our test coverage. Among
other things you can use it to query lldb for symbol files, for object files
and breakpoints.

Obviously shell tests are great for testing the command line driver itself or
the subcomponents already exposed by lldb-test. But when it comes to LLDB's
vast functionality, most things can be tested both through the driver as well
as the Python API. For example, to test setting a breakpoint, you could do it
from the command line driver with ``b main`` or you could use the SB API and do
something like ``target.BreakpointCreateByName`` [#]_.

A good rule of thumb is to prefer shell tests when what is being tested is
relatively simple. Expressivity is limited compared to the API tests, which
means that you have to have a well-defined test scenario that you can easily
match with ``FileCheck``.

Another thing to consider are the binaries being debugged, which we call
inferiors. For shell tests, they have to be relatively simple. The
``dotest.py`` test framework has extensive support for complex build scenarios
and different variants, which is described in more detail below, while shell
tests are limited to single lines of shell commands with compiler and linker
invocations.

On the same topic, another interesting aspect of the shell tests is that there
you can often get away with a broken or incomplete binary, whereas the API
tests almost always require a fully functional executable. This enables testing
of (some) aspects of handling of binaries with non-native architectures or
operating systems.

Finally, the shell tests always run in batch mode. You start with some input
and the test verifies the output. The debugger can be sensitive to its
environment, such as the the platform it runs on. It can be hard to express
that the same test might behave slightly differently on macOS and Linux.
Additionally, the debugger is an interactive tool, and the shell test provide
no good way of testing those interactive aspects, such as tab completion for
example.

API Tests
`````````

API tests are located under ``lldb/test/API``. They are run with the
``dotest.py``. Tests are written in Python and test binaries (inferiors) are
compiled with Make. The majority of API tests are end-to-end tests that compile
programs from source, run them, and debug the processes.

As mentioned before, ``dotest.py`` is LLDB's testing framework. The
implementation is located under ``lldb/packages/Python/lldbsuite``. We have
several extensions and custom test primitives on top of what's offered by
`unittest2 <https://docs.python.org/2/library/unittest.html>`_. Those can be
found  in
`lldbtest.py <https://github.com/llvm/llvm-project/blob/main/lldb/packages/Python/lldbsuite/test/lldbtest.py>`_.

Below is the directory layout of the `example API test
<https://github.com/llvm/llvm-project/tree/main/lldb/test/API/sample_test>`_.
The test directory will always contain a python file, starting with ``Test``.
Most of the tests are structured as a binary being debugged, so there will be
one or more source files and a ``Makefile``.

::

  sample_test
  ├── Makefile
  ├── TestSampleTest.py
  └── main.c

Let's start with the Python test file. Every test is its own class and can have
one or more test methods, that start with ``test_``.  Many tests define
multiple test methods and share a bunch of common code. For example, for a
fictive test that makes sure we can set breakpoints we might have one test
method that ensures we can set a breakpoint by address, on that sets a
breakpoint by name and another that sets the same breakpoint by file and line
number. The setup, teardown and everything else other than setting the
breakpoint could be shared.

Our testing framework also has a bunch of utilities that abstract common
operations, such as creating targets, setting breakpoints etc. When code is
shared across tests, we extract it into a utility in ``lldbutil``. It's always
worth taking a look at  `lldbutil
<https://github.com/llvm/llvm-project/blob/main/lldb/packages/Python/lldbsuite/test/lldbutil.py>`_
to see if there's a utility to simplify some of the testing boiler plate.
Because we can't always audit every existing test, this is doubly true when
looking at an existing test for inspiration.

It's possible to skip or `XFAIL
<https://ftp.gnu.org/old-gnu/Manuals/dejagnu-1.3/html_node/dejagnu_6.html>`_
tests using decorators. You'll see them a lot. The debugger can be sensitive to
things like the architecture, the host and target platform, the compiler
version etc. LLDB comes with a range of predefined decorators for these
configurations.

::

  @expectedFailureAll(archs=["aarch64"], oslist=["linux"]

Another great thing about these decorators is that they're very easy to extend,
it's even possible to define a function in a test case that determines whether
the test should be run or not.

::

  @expectedFailure(checking_function_name)

In addition to providing a lot more flexibility when it comes to writing the
test, the API test also allow for much more complex scenarios when it comes to
building inferiors. Every test has its own ``Makefile``, most of them only a
few lines long. A shared ``Makefile`` (``Makefile.rules``) with about a
thousand lines of rules takes care of most if not all of the boiler plate,
while individual make files can be used to build more advanced tests.  

Here's an example of a simple ``Makefile`` used by the example test.

::

  C_SOURCES := main.c
  CFLAGS_EXTRAS := -std=c99

  include Makefile.rules

Finding the right variables to set can be tricky. You can always take a look at
`Makefile.rules <https://github.com/llvm/llvm-project/blob/main/lldb/packages/Python/lldbsuite/test/make/Makefile.rules>`_
but often it's easier to find an existing ``Makefile`` that does something
similar to what you want to do.

Another thing this enables is having different variants for the same test
case. By default, we run every test for all 3 debug info formats, so once with
DWARF from the object files, once with gmodules and finally with a dSYM on
macOS or split DWARF (DWO) on Linux. But there are many more things we can test
that are orthogonal to the test itself. On GreenDragon we have a matrix bot
that runs the test suite under different configurations, with older host
compilers and different DWARF versions.

As you can imagine, this quickly lead to combinatorial explosion in the number
of variants. It's very tempting to add more variants because it's an easy way
to increase test coverage. It doesn't scale. It's easy to set up, but increases
the runtime of the tests and has a large ongoing cost.

The key take away is that the different variants don't obviate the need for
focused tests. So relying on it to test say DWARF5 is a really bad idea.
Instead you should write tests that check the specific DWARF5 feature, and have
the variant as a nice-to-have.

In conclusion, you'll want to opt for an API test to test the API itself or
when you need the expressivity, either for the test case itself or for the
program being debugged. The fact that the API tests work with different
variants mean that more general tests should be API tests, so that they can be
run against the different variants.

Running The Tests
-----------------

.. note::

   On Windows any invocations of python should be replaced with python_d, the
   debug interpreter, when running the test suite against a debug version of
   LLDB.

.. note::

   On NetBSD you must export ``LD_LIBRARY_PATH=$PWD/lib`` in your environment.
   This is due to lack of the ``$ORIGIN`` linker feature.

Running the Full Test Suite
```````````````````````````

The easiest way to run the LLDB test suite is to use the ``check-lldb`` build
target.

By default, the ``check-lldb`` target builds the test programs with the same
compiler that was used to build LLDB. To build the tests with a different
compiler, you can set the ``LLDB_TEST_COMPILER`` CMake variable.

It is possible to customize the architecture of the test binaries and compiler
used by appending ``-A`` and ``-C`` options respectively to the CMake variable
``LLDB_TEST_USER_ARGS``. For example, to test LLDB against 32-bit binaries
built with a custom version of clang, do:

::

   $ cmake -DLLDB_TEST_USER_ARGS="-A i386 -C /path/to/custom/clang" -G Ninja
   $ ninja check-lldb

Note that multiple ``-A`` and ``-C`` flags can be specified to
``LLDB_TEST_USER_ARGS``.

Running a Single Test Suite
```````````````````````````

Each test suite can be run separately, similar to running the whole test suite
with ``check-lldb``.

* Use ``check-lldb-unit`` to run just the unit tests.
* Use ``check-lldb-api`` to run just the SB API tests.
* Use ``check-lldb-shell`` to run just the shell tests.

You can run specific subdirectories by appending the directory name to the
target. For example, to run all the tests in ``ObjectFile``, you can use the
target ``check-lldb-shell-objectfile``. However, because the unit tests and API
tests don't actually live under ``lldb/test``, this convenience is only
available for the shell tests.

Running a Single Test
`````````````````````

The recommended way to run a single test is by invoking the lit driver with a
filter. This ensures that the test is run with the same configuration as when
run as part of a test suite.

::

   $ ./bin/llvm-lit -sv tools/lldb/test --filter <test>


Because lit automatically scans a directory for tests, it's also possible to
pass a subdirectory to run a specific subset of the tests.

::

   $ ./bin/llvm-lit -sv tools/lldb/test/Shell/Commands/CommandScriptImmediateOutput


For the SB API tests it is possible to forward arguments to ``dotest.py`` by
passing ``--param`` to lit and setting a value for ``dotest-args``.

::

   $ ./bin/llvm-lit -sv tools/lldb/test --param dotest-args='-C gcc'


Below is an overview of running individual test in the unit and API test suites
without going through the lit driver.

Running a Specific Test or Set of Tests: API Tests
``````````````````````````````````````````````````

In addition to running all the LLDB test suites with the ``check-lldb`` CMake
target above, it is possible to run individual LLDB tests. If you have a CMake
build you can use the ``lldb-dotest`` binary, which is a wrapper around
``dotest.py`` that passes all the arguments configured by CMake.

Alternatively, you can use ``dotest.py`` directly, if you want to run a test
one-off with a different configuration.

For example, to run the test cases defined in TestInferiorCrashing.py, run:

::

   $ ./bin/lldb-dotest -p TestInferiorCrashing.py

::

   $ cd $lldb/test
   $ python dotest.py --executable <path-to-lldb> -p TestInferiorCrashing.py ../packages/Python/lldbsuite/test

If the test is not specified by name (e.g. if you leave the ``-p`` argument
off),  all tests in that directory will be executed:


::

   $ ./bin/lldb-dotest functionalities/data-formatter

::

   $ python dotest.py --executable <path-to-lldb> functionalities/data-formatter

Many more options that are available. To see a list of all of them, run:

::

   $ python dotest.py -h


Running a Specific Test or Set of Tests: Unit Tests
```````````````````````````````````````````````````

The unit tests are simple executables, located in the build directory under ``tools/lldb/unittests``.

To run them, just run the test binary, for example, to run all the Host tests:

::

   $ ./tools/lldb/unittests/Host/HostTests


To run a specific test, pass a filter, for example:

::

   $ ./tools/lldb/unittests/Host/HostTests --gtest_filter=SocketTest.DomainListenConnectAccept


Running the Test Suite Remotely
```````````````````````````````

Running the test-suite remotely is similar to the process of running a local
test suite, but there are two things to have in mind:

1. You must have the lldb-server running on the remote system, ready to accept
   multiple connections. For more information on how to setup remote debugging
   see the Remote debugging page.
2. You must tell the test-suite how to connect to the remote system. This is
   achieved using the ``--platform-name``, ``--platform-url`` and
   ``--platform-working-dir`` parameters to ``dotest.py``. These parameters
   correspond to the platform select and platform connect LLDB commands. You
   will usually also need to specify the compiler and architecture for the
   remote system.

Currently, running the remote test suite is supported only with ``dotest.py`` (or
dosep.py with a single thread), but we expect this issue to be addressed in the
near future.

Running tests in QEMU System Emulation Environment
``````````````````````````````````````````````````

QEMU can be used to test LLDB in an emulation environment in the absence of
actual hardware. `QEMU based testing <https://lldb.llvm.org/use/qemu-testing.html>`_
page describes how to setup an emulation environment using QEMU helper scripts
found under llvm-project/lldb/scripts/lldb-test-qemu. These scripts currently
work with Arm or AArch64, but support for other architectures can be added easily.

Debugging Test Failures
-----------------------

On non-Windows platforms, you can use the ``-d`` option to ``dotest.py`` which
will cause the script to print out the pid of the test and wait for a while
until a debugger is attached. Then run ``lldb -p <pid>`` to attach.

To instead debug a test's python source, edit the test and insert
``import pdb; pdb.set_trace()`` at the point you want to start debugging. In
addition to pdb's debugging facilities, lldb commands can be executed with the
help of a pdb alias. For example ``lldb bt`` and ``lldb v some_var``. Add this
line to your ``~/.pdbrc``:

::

   alias lldb self.dbg.HandleCommand("%*")

::

Debugging Test Failures on Windows
``````````````````````````````````

On Windows, it is strongly recommended to use Python Tools for Visual Studio
for debugging test failures. It can seamlessly step between native and managed
code, which is very helpful when you need to step through the test itself, and
then into the LLDB code that backs the operations the test is performing.

A quick guide to getting started with PTVS is as follows:

#. Install PTVS
#. Create a Visual Studio Project for the Python code.
    #. Go to File -> New -> Project -> Python -> From Existing Python Code.
    #. Choose llvm/tools/lldb as the directory containing the Python code.
    #. When asked where to save the .pyproj file, choose the folder ``llvm/tools/lldb/pyproj``. This is a special folder that is ignored by the ``.gitignore`` file, since it is not checked in.
#. Set test/dotest.py as the startup file
#. Make sure there is a Python Environment installed for your distribution. For example, if you installed Python to ``C:\Python35``, PTVS needs to know that this is the interpreter you want to use for running the test suite.
    #. Go to Tools -> Options -> Python Tools -> Environment Options
    #. Click Add Environment, and enter Python 3.5 Debug for the name. Fill out the values correctly.
#. Configure the project to use this debug interpreter.
    #. Right click the Project node in Solution Explorer.
    #. In the General tab, Make sure Python 3.5 Debug is the selected Interpreter.
    #. In Debug/Search Paths, enter the path to your ninja/lib/site-packages directory.
    #. In Debug/Environment Variables, enter ``VCINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\``.
    #. If you want to enabled mixed mode debugging, check Enable native code debugging (this slows down debugging, so enable it only on an as-needed basis.)
#. Set the command line for the test suite to run.
    #. Right click the project in solution explorer and choose the Debug tab.
    #. Enter the arguments to dotest.py.
    #. Example command options:

::

   --arch=i686
   # Path to debug lldb.exe
   --executable D:/src/llvmbuild/ninja/bin/lldb.exe
   # Directory to store log files
   -s D:/src/llvmbuild/ninja/lldb-test-traces
   -u CXXFLAGS -u CFLAGS
   # If a test crashes, show JIT debugging dialog.
   --enable-crash-dialog
   # Path to release clang.exe
   -C d:\src\llvmbuild\ninja_release\bin\clang.exe
   # Path to the particular test you want to debug.
   -p TestPaths.py
   # Root of test tree
   D:\src\llvm\tools\lldb\packages\Python\lldbsuite\test

::

   --arch=i686 --executable D:/src/llvmbuild/ninja/bin/lldb.exe -s D:/src/llvmbuild/ninja/lldb-test-traces -u CXXFLAGS -u CFLAGS --enable-crash-dialog -C d:\src\llvmbuild\ninja_release\bin\clang.exe -p TestPaths.py D:\src\llvm\tools\lldb\packages\Python\lldbsuite\test --no-multiprocess

.. [#] `https://lldb.llvm.org/python_reference/lldb.SBTarget-class.html#BreakpointCreateByName <https://lldb.llvm.org/python_reference/lldb.SBTarget-class.html#BreakpointCreateByName>`_
