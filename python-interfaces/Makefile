#
#   Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
#
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this file,
#   You can obtain one at http://mozilla.org/MPL/2.0/.
#

#******************************************************************************
#
#	M a k e f i l e
#
#    This file is a sample "makefile" that can be customized to build most
#    executables and libraries that a user might want.  In most cases, the
#    user need only define the "GOALS" and the version numbers for his output
#    files and the rest of the required infrastructure will be provided by the
#    master makefile that is included here.
#
#    To customize the operation of this makefile, follow the directions for
#    setting the various parameters below and for more complex custom builds,
#    see the source of the master makefile to determine what can be modified
#    and/or overridden.
#
#    Most of the variables defined in the master makefile can be replaced or
#    added to.  To replace a variable completely use:
#
#        VAR := value
#    or:
#        VAR  = value
#
#    If you don't know the difference between these two forms, please see the
#    GNU Make User's Guide.  When in doubt, use the "=" form.  When replacing
#    a variable, be sure to define the new value after the master makefile has
#    been included in the client makefile.
#
#    To add to a list (such as a list of libraries to be linked), use the
#    "+=" operator:
#
#        LD_LIBS += -levent -ldl
#
#    The master makefile will automatically generate a version string into
#    each "goal" file specified by the user.  This string will contain the
#    name of the "goal" file, the version number specified in this file, the
#    date of the build, the user performing the build, and the hostname of the
#    machine that it was built on.
#
#    The master makefile will automatically generate and maintain all of the
#    file dependencies by generating a dependency file with a ".d" extension
#    into the dependency directory (normally ".deps").  These dependency files
#    may be viewed if desired.  Updating a header file will automatically
#    cause all of the source files that include that header to be rebuilt and
#    so forth.  Note that the dependency directories will be created in the
#    object directory tree if a separate tree has been specified.
#
#    All of the header files with ".h" extensions will be used and the
#    dependencies between the sources and headers will be automatically
#    generated.  The list of C, C++, and header files can be changed if the
#    user wishes.  See the details below to determine which variables to
#    supplement and/or override.
#
#    The makefile will automatically operate in the "parallel" build mode.
#    This allows the make system to run multiple compile/link jobs
#    simultaneously while it keeps track of order dependencies and
#    synchronization points and so forth.  This speeds up the build times
#    considerably on systems with more than one processor.  On a 2 processor
#    system for instance, the build time is cut almost in half by running
#    parallel compiles on the 2 processors simultaneously.  Note that because
#    of this feature, the user may see some build messages printed "out of
#    sequence" but all operations will be completed correctly.  This feature
#    can be disabled if desired by defining the variable "NO_PARALLEL_BUILDS".
#
#    If the user defines the variable "SUBDIRS" to be a list of
#    subdirectories, the make process will be recursively invoked in each of
#    those subdirectories before the current directory is built.
#
#    If the user has trouble with some features of the build, there are some
#    build debugging aids provided in the master makefile.   See the
#    description of these and how to use them in the master makefile.
#
#    The built-in targets of the master makefile are:
#
#    all - Build everything defined in the makefile.  This is not quite the
#        same as specifying "make" with no targets.  If specified explicitly,
#        it will cause the current directory to not be called recursively with
#        the "MAKE_OPTIONS" variable.  This can be handy if the user wants to
#        execute the make in the currently directory with the parallel build
#        feature disabled.  Note that this will not work the same way for any
#        subdirectories.  Any subdirectories specified by the user will be
#        built normally with all of the "MAKE_OPTIONS" specified when they are
#        built.
#
#    xxx - Where "xxx" is the name of the "goal" for this build.  This does
#        not normally need to be specified as it is the default goal of the
#        makefile.  This can be built with "make" or "make all" or "make xxx".
#        This form of the make command is useful if the user has more than one
#        "goal" defined and he only wants to build one of them.
#
#    clean - Remove all object files and "goal" files.  This will not remove
#        any directories that have been created or any of the dependency
#        files.  It will cause all of the source files to be recompiled and
#        linked the next time "make" is invoked.
#
#    distclean - "Distribution" clean - Remove all of the files that "clean"
#        does plus remove the working directories that contain the dependency
#        files and the object directory if one was created.  This target
#        should return the system to it's "orginal" state.
#
#    install - Execute any installation commands that the user has defined in
#        this makefile.
#
#******************************************************************************

#
#    Define the debugging and special configuration flags.  All of these flags
#    should normally not be defined.  An explanation of what each does can be
#    found in the master makefile (master.mk).
#
# NO_PARALLEL_BUILDS = 1
# TRACE_COMMANDS = 1
# TRACE_RULES = 1
# NO_DIR_OUTPUT = 1
# NO_AUTO_SUBDIRS = 1
# NO_AUTO_INSTALL = 1

#
#    Set the "TOP" environment variable to be the location of the top level
#    source directory in the current branch.  This is done by executing a bit
#    of shell code that traverses up the directory tree until it finds a
#    directory containing the marker file.  That directory is the root of the
#    current code tree and "TOP" will be set to that directory.
#
#    Note that we only do this if the user has not set the value explicitly.
#    This variable should always be set by the time we get to this point in
#    the makefile processing since this file is included from the client
#    makefiles after TOP has already been set in the client makefile.  We set
#    it here just for some added peace of mind in case some developer does
#    something strange and does not set the variable in his client makefile.
#
MARKER_FILE = .treeTop

ifeq "$(TOP)" ""
	findTOP = \
		dir=$$PWD; \
		while [ "$$dir" != "/" ]; \
		do \
			if [ -f $$dir/$(MARKER_FILE) ]; \
			then \
				echo $$dir; \
				exit 0; \
			fi; \
			dir=$$(dirname $$dir); \
		done;

	export TOP = $(shell $(findTOP))
endif

#
#    If we failed to set the "TOP" environment variable properly, issue an
#    error message and abort the make processing.  This is a fatal error and
#    should only happen if the current directory is not a valid source tree
#    directory or the marker file has been deleted in the current branch root
#    directory.
#
ifeq "$(TOP)" ""
    $(error Could not set the "TOP" environment variable - Aborting!)
    exit 2
endif

#
#    Define the goals of this makefile.
#
#    There are several types of files that can be automatically built.  The
#    most basic is an executable file.  All executables to be build from the
#    current directory should be specified in the "GOALS" variable:
#
#        GOALS = executable1 executable2 ...
#
#    Shared library files can be build by specifying them in the "SO_GOALS"
#    variable:
#
#        SO_GOALS = sharedLibrary1.so sharedLibrary2.so ...
#
#    Archive libraries can be built by specifying the "AR_GOALS" variable:
#
#        AR_GOALS = archiveLibrary1.a archiveLibrary2.a ...
#
#    Note that this definition MUST occur before the include of the global
#    makefile.
#
#    If the user does not define any of these variables, nothing will be built
#    in the current directory.  This can be a valid thing to do if there are
#    subdirectories that need to be built but no code in the top level
#    directory.
#
# GOALS    = mux
# SO_GOALS = libsharedLibrary.so
# AR_GOALS = archiveLibrary.so

SO_GOALS = libpy_vsi.so

#
#    Define the list of alternate source directories.  This is a space or colon
#    separated list of directory names that the system will search for source
#    files.  Note that object files will still go into the current directory
#    or the "OBJ_DIR" if the user has specified it.
#
#    To set the source directory path, define the "SRC_DIRS" variable:
#
# SRC_DIRS = $(TOP)/src:../util

#
#    The object files that are needed for each source file must be specified
#    as follows.  The user must define a variable that is the prefix "OBJS_"
#    followed by the name of the executable for which these objects will be
#    used.  For example:
#
#        OBJS_executable1 = objectFile1 objectFile2 ...
#        OBJS_sharedLibrary1 = objectFile3 objectFile4 ...
#        ...etc.
#
#    Note that in the case of goals that have an extension (such as ".so" and
#    ".a"), the extension is dropped when they are used to form the name of
#    other variables.
#
#    To make life a bit easier, there are predefined macros that contains
#    lists of object files:
#
#        CC_OBJECTS - All of the objects derived from C source files in the
#                     current directory and all of the "SRC_DIRS" directories.
#
#        CXX_OBJECTS - All of the objects derived from C++ source files in the
#                      current directory and all of the "SRC_DIRS"
#                      directories.
#
#        OBJECTS - The combination of the "CC_OBJECTS" and "CXX_OBJECTS"
#                  lists.  This is the list of all C and C++ object files in
#                  the current and "SRC_DIRS" directories.
#
#    If there is only a single goal of a specified type that is created from
#    all of the objects then the user does not need to specify the object
#    list, it will automatically be created for him.
#
#    For example, if the user specifies a single executable called "testJson"
#    that is linked from all of the available object files he could write:
#
#        OBJS_testJson = $(OBJECTS)
#
#    This would be fine but in cases like this, it is not necessary.  The make
#    system will automatically build the above rule if there is only a single
#    goal file.  Note that a user could specify an executable, a shared
#    library, and an archive file that all depend on all of the object files
#    and he would still not need to specify anything here.
#
#    If the user does specify an object list for a goal, that list will always
#    be used, even if the goal is a single goal of a particular type.
#
# OBJS_testJson = testJson.o

OBJS_libpy_vsi = vsi_py.o

#
#    Define the directory to be used to store object files and "goal" files.
#    This MUST occur before the include of the global makefile.  This is
#    optional and if not set, objects and executables will be generated into
#    the current directory.
#
# OBJ_DIR = obj

#
#    Define the directory to be used to store dependency files.  This is
#    optional and if not set, dependency files will be generated into a
#    directory called ".deps".  If putting the dependencies into the current
#    directory is desired, specify this option with "." as the directory.
#
# DEPS_DIR = /some/path/to/a/directory

#
#    If there are any subdirectories that should also be built, define the
#    list of subdirectories here.  This is optional but if it is defined, it
#    must be defined before the include of the global makefile.
#
#    Any subdirectories specified here will be built before the current
#    directory is built.
#
# SUBDIRS += foo bar

#
#    If there are any goals that should be added to the default list of goals
#    that gets built when make is invoked with no arguments, specify them in
#    the following variable.  If this is used, the user must also add the rule
#    for building this extra goal at the end of this makefile.
#
#    This variable is optional but it must be defined before the master
#    makefile is included.  The rule to build this goal must be defined after
#    the makefile is included.
#
# DEFAULT_GOALS := someNewGoal

#
#    If there is a "doxygen" configuration file that is defined for the
#    current build, specify the path and name of that file here.  The
#    "doxygen" program will not be run automatically but it can be invoked by
#    building the special target "doxygen" (i.e. "make doxygen").  The
#    standard name of the doxygen definition fiel is "Doxyfile" but it can be
#    given any name the user likes.  This file can be created manually with a
#    text editor or an easier way to create/modify it is by using the
#    "doxywizard" UI.
#
# DOXYGEN_FILE = Doxyfile

#
#    Include the master makefile here.
#
include $(TOP)/build/master.mk

#
#    If there are multiple targets being built here and there are dependencies
#    between the targets, those dependencies should be specified here.  A
#    simple rule such as:
#
#        target1: target2 target3
#
#    Will cause target1 to be dependent on targets 2 and 3 and targets 2 and
#    3 will be built before target1 is built.
#
#    One common case of this is when a directory defines both a library and
#    and executable that is linked with that library.  In a case like this, be
#    sure to do something like:
#
#        executable: library
#
#    This will ensure that the library gets built before the executable is
#    linked.
#
#    One indication that this might be necessary is if your make does not work
#    the first time you run it but if you run it multiple times, it eventually
#    builds everything correctly.  This can be confirmed by setting the
#    "NO_PARALLEL_BUILDS = 1" flag and building again.  If the build succeeds
#    on the first try then you definitely have some dependencies that need to
#    be specified here.
#

#
#    Define the compiler/linker optimization level.  If this variable is not
#    defined, it will default to the the optimization level defined in the
#    master makefile which is normally a debug build.  If the user wants a
#    non-debuggable "goal" file, this variable should be set to something like
#    "-O3".
#
#    Note that if a flag only needs to be set for a specific compiler or
#    linker, it can be defined just for the C compiler, C++ compiler, or
#    linker.  See the master makefile for the variables that need to be
#    overridden to do this for this specific application.
#
# OPTIMIZATION = -O3

#
#    Define additional include files here.  By default, all of the include
#    files in the current directory will be used.  If additional files are
#    needed, they can be added to the list as in the following example.  This
#    variable is optional and should be rarely needed.
#
# INCLUDES += ../some/path/somefile.h someOtherFile.hpp

#
#    Define additional directories to search for include files here.  By
#    default, the current directory and all "usual" directories will be
#    automatically included.  Only unusual directories need be specified here.
#    This variable is optional.
#
# INCLUDE_DIRS += -I../some/path/to/include/files

#
# Add a define so that we can compile this for Python 2 and Python 3
#
ifndef PYTHON_VERSION
	python_version_full := $(wordlist 2,4,$(subst ., ,$(shell python --version 2>&1)))
	python_version_major := $(word 1,${python_version_full})
	python_version_minor := $(word 2,${python_version_full})
	python_version_patch := $(word 3,${python_version_full})

	PYTHON_VERSION ?= ${python_version_major}.${python_version_minor}

	ifeq ($(python_version_major),3)
		PYTHON_VERSION := ${PYTHON_VERSION}m
	endif
endif

DEFINES += -DPYTHON_VERSION=$(PYTHON_VERSION)

#
#    Define any additional link flags that need to be used to form the "goal"
#    files.  By default, all of the "usual" flags will be automatically
#    included.  Only unusual flags need be specified here.  This variable
#    is optional.  Note that the variable names here consist of the prefix
#    "LD_OPTS_" followed by the name of the goal that they are to apply to.
#
# LD_OPTS_testJson += -pg
# SO_OPTS_testJson += -pg
# AR_OPTS_testJson += -pg

#
#    Define any additional libraries that need to be linked to form the "goal"
#    files.  By default, all of the "usual" libraries will be automatically
#    included.  Only unusual libraries need be specified here.  This variable
#    is optional.  Note that the variable names here consist of the prefix
#    "LD_LIBS_" followed by the name of the goal that they are to apply to.
#
# LD_LIBS_testJson += -L../someDirectory -ljunk -lpthreads
# SO_LIBS_testJson += -L../someDirectory -ljunk -lpthreads
# AR_LIBS_testJson += -L../someDirectory -ljunk -lpthreads

#
#    Define any files that need to be removed when the "make clean" command is
#    issued.  All of the derived files such as object files and executables
#    and/or the "goals" file are automatically removed by a "clean" operation.
#    If there are additional files the user wishes removed, specify them here.
#
# CLEAN_FILES += someFile someOtherFile *.qt

#
#    This rule will install the goal executables and libraries into the top
#    level directories.  This rule will be executed automatically as a default
#    rule (i.e. if the user types "make" with no arguments) unless the user
#    explicitly disables this feature by defining the "NO_AUTO_INSTALL"
#    variable.
#
#    There are no default files in this section.  They must all be supplied by
#    the user if desired.  If the user does not specify anything here,
#    invoking "make install" will not do anything.
#
#    Each target below will result in a file being installed into the named
#    directory.  For example, to install a file called gorph.so into the top
#    level library directory $(TOP_LIB), specify:
#
#        INSTALL_LIB_FILES = gorph.so
#
#    To install the executables "gorph" and "gorphTest" into the top level bin
#    directory, specify:
#
#        INSTALL_BIN_FILES = gorph gorphTest
#
#    Each target below can be given a list of blank separated files.  The
#    files being specified do not need to have the object directory specified
#    for them if there is one defined, the rules here will figure that out
#    automatically.
#
#    All of the defined goal files including any additional "DEFAULT_GOALS"
#    files that have been defined are dependencies for this rule.  This
#    ensures that all of the goals have been built before this rule tries to
#    install any of them.
#
#INSTALL_BIN_FILES       = ""
#INSTALL_PLAIN_BIN_FILES = ""
#INSTALL_LIB_FILES       = ""
#INSTALL_PLAIN_LIB_FILES = ""
#INSTALL_PLAIN_INCLUDE_FILES = $(wildcard *.h)

INSTALL_LIB_FILES = libpy_vsi.so

#
#    Define the following macro if additional dependencies are needed for the
#    install rule.  All of the goals that have been defined are already
#    included as dependencies so these do not need to be specified here but if
#    there are any files that will be used by user defined INSTALL_COMMANDS
#    that are not part of the goal list, those files need to be included in
#    this macro.
#
#    Defining the following variable will guarantee that the install rule does
#    not try to run before the file it is going to use has finished building.
#
# INSTALL_DEPS += someOddFile

