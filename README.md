![Build](https://github.com/abrt/satyr/workflows/Build/badge.svg)

## About Satyr

Failures of computer programs are omnipresent in the information technology
industry: they occur during software development, software testing, and also in
production.  Failures occur in programs from all levels of the system stack.
The program environment differ substantially between kernel space, user space
programs written in C or C++, Python scripts, and Java applications, but the
general structure of failures is surprisingly similar between the mentioned
environments due to imperative nature of the languages and common concepts such
as procedures, objects, exceptions.

Satyr is a collection of low-level algorithms for program failure processing,
analysis, and reporting supporting kernel space, user space, Python, and Java
programs.  Considering failure processing, it allows to parse failure
description from various sources such as GDB-created stack traces, Python stack
traces with a description of uncaught exception, and kernel oops message.
Information can also be extracted from the core dumps of unexpectedly
terminated user space processes and from the machine executable code of
binaries.  Considering failure analysis, the stack traces of failed processes
can be normalized, trimmed, and compared.  Clusters of similar stack traces can
be calculated.  In multi-threaded stack traces, the threads that caused the
failure can be discovered.  Considering failure reporting, the library can
generate a failure report in a well-specified format, and the report can be
sent to a remote machine.

Due to the low-level nature of the library and implementors' use cases, most of
its functionality is currently limited to Linux-based operating systems using
ELF binaries.  The library can be extended to support Microsoft Windows and OS
X platforms without changing its design, but dedicated engineering effort would
be required to accomplish that.

## How to build Satyr from source

### Install dependencies:

    $ sudo dnf builddep --spec satyr.spec

or

    $ ./autogen.sh sysdeps --install

### Configure:

    $ ./autogen.sh
    $ ./configure

### Build and install:

Directly:

    $ make && sudo make install

With rpms:

    $ make rpm
    $ sudo dnf install build/x86_64/*rpm

## Contributing a change

### Basic git workflow:

1. Fork the Satyr repository (hit fork button on https://github.com/abrt/satyr)

2. Clone your fork

3. Create and check out to a new branch in your clone (`git checkout -b <name_of_branch>`)

4. ... make changes...

5. Test your changes with `make check` and `make check-valgrind`

6. Create tests for the given changes

7. Add edited files (`git add <file_name>`)

8. Create commit (`git commit`) [How to write a proper git commit
   message](https://chris.beams.io/posts/git-commit/)  
Note: You can set up a helpful commit message template for your text editor by running
`$ git config commit.template .git-commit-template`. Remember though that
committing with `git commit -m` defeats its purpose. You might want to rethink
your habits.

9. Push your branch (`git push -u origin <name_of_branch>`)

10. Go to https://github.com/abrt/satyr and click `Compare & pull request`

11. Create the PR

12. Wait for review
