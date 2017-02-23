#******************************************************************************
#
#	m a s t e r . m k
#
#    Copyright (C) 2007 Mies Ventures, Inc. All rights reserved.
#
#    The information contained herein is confidential and proprietary to Mies
#    Ventures, Inc., and is considered a trade secret as defined in section
#    499C of the California Penal Code. Use of this information by anyone
#    other than authorized employees of Mies Ventures is granted only under a
#    written non-disclosure agreement expressly prescribing the scope and
#    manner of such use.
#
#
#    This is the master makefile.  It contains all of the commands and
#    definitions required to compile and link code modules.
#
#    This file must be included in all of the user defined "makefile" files.
#
#    There is a sample user "makefile" provided that can be modified according
#    to the instructions in the sample for the user's application.
#
#    The user's makefile can be customized to build most executables and
#    libraries that a user might want.  In most cases, the user need only
#    define the "GOALS" and the version numbers for his output files and the
#    rest of the required infrastructure will be provided by the master
#    makefile.
#
#    To customize the operation of this makefile, follow the directions for
#    setting the various parameters below and for more complex custom builds,
#    see the comments in the body of this makefile to determine which
#    variables can be replaced and/or overridden.
#
#    General Guidelines:
#    -------------------
#
#    As a general rule, if a pathname is needed somewhere, use a relative path
#    if possible (e.g. "../src").  If that is not available or it's very ugly,
#    use a path relative to the "TOP" define (e.g. "$(TOP)/../src").  If all
#    else fails, use an absolute path but this should be a last resort.
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
#    The master makefile will automatically generate and maintain all of the
#    file dependencies by generating a dependency file with a ".d" extension
#    into the dependency directory (normally ".deps").  These dependency files
#    may be viewed if desired.  Updating a header file will automatically
#    cause all of the source files that include that header to be rebuilt and
#    so forth.
#
#    There are several types of files that can be automatically built.  The
#    most basic is an executable file.  All executables to be built from the
#    current directory should be specified in the "GOALS" variable:
#
#        GOALS = executable1 executable2 ...
#
#    Shared library files can be built by specifying them in the "SO_GOALS"
#    variable:
#
#        SO_GOALS = libsharedLibrary1.so libsharedLibrary2.so ...
#
#    Archive libraries can be built by specifying the "AR_GOALS" variable:
#
#        AR_GOALS = archiveLibrary1.a archiveLibrary2.a ...
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
#    Similarly the user can define additional link options and link libraries
#    by defining variables like:
#
#        LD_OPTS_executable1       = -pg
#        SO_OPTS_libsharedLibrary1 = -pg
#        AR_OPTS_archiveLibrary1   = -pg
#
#        LD_LIBS_executable1       = -L../someDirectory -ljunk -lpthreads
#        SO_LIBS_libsharedLibrary1 = -L../someDirectory -ljunk -lpthreads
#        AR_LIBS_archiveLibrary1   = -L../someDirectory -ljunk -lpthreads
#
#    All of the header files with ".h" extensions will be used and the
#    dependencies between the sources and headers will be automatically
#    generated.  The list of C, C++, and header files can be changed if the
#    user wishes.  See the details below to determine which variables to
#    supplement and/or override.
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
#    so forth.
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
#    those subdirectories before the current directory is built.  This results
#    in a "depth first" traversal of the build tree.
#
#    The user may replace or add to virtually any of the variables defined
#    here.  See the header of the sample makefile for more information about
#    replacing or adding to variables.
#
#    If the user has trouble with some features of the build, there are some
#    build debugging aids provided at the end of this file.   See the
#    description of these and how to use them at the end of this file.
#
#    The built-in targets of the master makefile are:
#
#    all - Build everything defined in the makefile.  This is normally the
#        same as specifying "make" with no targets.
#
#    xxx - Where "xxx" is the name of the "goal" for this build.  This does
#        not normally need to be specified as it is the default goal of the
#        makefile.  This can be built with "make" or "make all" or "make xxx".
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
#    rebuild - Perform a complete rebuild of the current subtree.  This will
#        unconditionally recompile and relink everything without regard to the
#        dates on the various files.  This can be used if the user suspects a
#        problem in the make process that results in some files not being
#        regenerated as they should be or when the user just wants to perform
#        a complete build in preparation for distribution.
#
#******************************************************************************

#
#    Set the "TOP" environment variable to be the location of the marker file
#    in the current branch.  This is done by executing a bit of shell code
#    that traverses up the directory tree until it finds a directory
#    containing the marker file.  The marker file is also defined below and
#    can be changed if desired. That directory is the root of the current code
#    tree and "TOP" will be set to the directory containing the marker file.
#
#    Note that we only do this if the user has not set the value explicitly.
#    This variable should always be set by the time we get to this point in
#    the makefile processing since this file is included from the client
#    makefiles after TOP has already been set in the client makefile.  We set
#    it here just for some added peace of mind in case some developer does
#    something strange and does not set the variable in his client makefile.
#
MARKER_FILE = .treeTop

findTOP = \
    dir=$$PWD; \
    while [ "$$dir" != "/" ]; \
    do \
        if [ -f $$dir/$(MARKER_FILE) ]; \
        then \
            echo $$dir; \
            exit 0; \
        fi; \
        dir=`dirname $$dir`; \
    done \

export TOP = $(shell $(findTOP))

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
#    Define some handy macros.
#
EMPTY  :=
SPACE  := $(EMPTY) $(EMPTY)
INDENT := $(EMPTY)    $(EMPTY)

#
#    Define the root of the current branch and the branch name.  The "ROOT" is
#    the directory just above "TOP" (i.e. $(TOP)/..).
#
ROOT := $(shell dirname $(TOP))

#
#	If the following is defined, versioning will be used, otherwise,
#	versioning is disabled.
#
# ENABLE_VERSIONING = 1

ifdef ENABLE_VERSIONING
#
#    Define the version number for all of the executables and libraries we
#    will be building.
#
#    The major, minor, patch, and build numbers will be concatenated together
#    with a period separating them to produce a string that looks something
#    like:
#
#        5.0.1.4562
#

#
#    Explicitly create the BUILD_NUMBER file if it does not exist.
#
$(shell                                 \
if [ ! -w $(TOP)/build/BUILD_NUMBER ];  \
then                                    \
	echo 1 > $(TOP)/build/BUILD_NUMBER; \
fi )

#
#	Get all of the current version numbers from the build files.
#
MAJOR_VER = $(shell cat $(TOP)/build/MAJOR_VERSION)
MINOR_VER = $(shell cat $(TOP)/build/MINOR_VERSION)
PATCH_VER = $(shell cat $(TOP)/build/PATCH_VERSION)
BUILD_NUM = $(shell cat $(TOP)/build/BUILD_NUMBER)

#
#    Define the actual version strings.
#
#    The "VERSION_SUFFIX" will be used to append to the name of libraries and
#    executables that are generated.
#
BASE_VERSION   = $(MAJOR_VER).$(MINOR_VER).$(PATCH_VER)
VERSION        = $(MAJOR_VER).$(MINOR_VER).$(PATCH_VER).$(BUILD_NUM)
VERSION_SUFFIX = -$(BASE_VERSION)

#
#    The following strings are used to construct the library version suffixes.
#
MAJOR_SFX = $(MAJOR_VER)
MINOR_SFX = $(MAJOR_VER).$(MINOR_VER)
PATCH_SFX = $(MAJOR_VER).$(MINOR_VER).$(PATCH_VER)

#
#    Define the root name of the temporary version files that are generated by
#    the makefile for each goal defined.  These version files will be deleted
#    before the build is finished and should never be seen by the user.
#
VERSION_FILE := _version_

#
#    The version string can't just be defined as a static string because the
#    compiler decides that it's not used anywhere in the file and optimizes it
#    out completely.
#
#    If it's defined as a const string, the compiler will warn that it's not
#    used anywhere.
#
#    Defining it inside a function keeps the compiler and linker happy!
#
CURRENT_DATE    = $(shell date)

VERSION_STRING  = 'const char* $*_version ( void )'
VERSION_STRING += '{'
VERSION_STRING += '    const char* VERSION_STRING = "$(VERSION)";'
VERSION_STRING += '    return VERSION_STRING;'
VERSION_STRING += '}'
VERSION_STRING += 'const char* $*_version_all ( void )'
VERSION_STRING += '{'
VERSION_STRING += '    const char* CODE_VER = "CODE_VER: $* $(VERSION)'
VERSION_STRING += 'built $(CURRENT_DATE) by $(USER) on $(HOSTNAME)";'
VERSION_STRING += '    return CODE_VER;'
VERSION_STRING += '}'
VERSION_STRING += 'const char* $*_sdk_version ( void )'
VERSION_STRING += '{'
VERSION_STRING += '    const char* SDK_VER_STRING = "$(BASE_VERSION)";'
VERSION_STRING += '    return SDK_VER_STRING;'
VERSION_STRING += '}'

endif

#
#    Define the list of alternate source directories.  This is a colon
#    separated list of directory names that the system will search for source
#    files.  Note that object files will still go into the current directory
#    or the "OBJ_DIR" if the user has specified it.
#
#    To set the source directory path, define the "SRC_DIRS" variable:
#
#        SRC_DIRS = ../someDir:$(TOP)/util
#
VPATH = $(subst $(SPACE),:,$(strip $(SRC_DIRS)))

#
#    Define the lists of source files.
#
#    Note that these lists include all of the files in all of the "SRC_DIRS"
#    directories as well as the current directory.
#
CC_SOURCES := $(wildcard *.c)

$(foreach d,$(subst :, ,$(SRC_DIRS)), \
            $(eval CC_SOURCES += $(notdir $(wildcard $(d)/*.c))))

CXX_SOURCES := $(wildcard *.cpp)

$(foreach d,$(subst :, ,$(SRC_DIRS)), \
            $(eval CXX_SOURCES += $(notdir $(wildcard $(d)/*.cpp))))

SOURCES := $(CXX_SOURCES) $(CC_SOURCES)

#
#    Define the list of include directories.
#
INCLUDE_DIRS := -I. -I.. -I$(TOP)/include

#
#    Define the lists of object files.
#
CC_OBJECTS  := $(subst .c,.o,$(CC_SOURCES) )
CXX_OBJECTS := $(subst .cpp,.o,$(CXX_SOURCES) )
OBJECTS     := $(CXX_OBJECTS) $(CC_OBJECTS)

#
#    If the user has only defined one goal in each category and he has not
#    defined an explicit object list for that goal, create the object list
#    for that goal that consists of all objects defined in the current
#    context.  This will include all object files derived from sources in the
#    current directory and all "SRC_DIRS" defined by the user.
#
ifeq "$(words $(GOALS))" "1"
    OBJS_$(notdir $(basename $(GOALS))) ?= $(OBJECTS)
endif

ifeq "$(words $(SO_GOALS))" "1"
    OBJS_$(notdir $(basename $(SO_GOALS))) ?= $(OBJECTS)
endif

ifeq "$(words $(AR_GOALS))" "1"
    OBJS_$(notdir $(basename $(AR_GOALS))) ?= $(OBJECTS)
endif

#
#    The following variable defines where the dependency files are located.
#    This will normally be in a subdirectory called ".deps".  If putting the
#    dependencies into the current directory is desired, specify this option
#    with "." as the directory.
#
DEPS_DIR ?= .deps

#
#    The "DEP_ROOT" macro is defined to be the path to the dependency file
#    with the complete path and the root of the file name but without the file
#    extension.  This is used in the dependency generation code later and is
#    mostly for neatness.
#
DEP_ROOT = $(DEPS_DIR)/$(*F)

#
#    If the user did define an object directory, rename each of the goal files
#    to be located in the object directory. This will insert the object file
#    directory after any pathname and before the goal file in each file
#    specified in the below lists.
#
ifdef OBJ_DIR
    OBJ_DIR  := $(addsuffix /,$(OBJ_DIR))
    GOALS    := $(strip $(join $(addsuffix $(OBJ_DIR),$(dir $(GOALS))),\
                $(notdir $(GOALS))))
    SO_GOALS := $(strip $(join $(addsuffix $(OBJ_DIR),$(dir $(SO_GOALS))),\
                $(notdir $(SO_GOALS))))
    AR_GOALS := $(strip $(join $(addsuffix $(OBJ_DIR),$(dir $(AR_GOALS))),\
                $(notdir $(AR_GOALS))))
endif

#
#    For each of the possible libraries, append the version number suffix to
#    the names of the goal files.
#
#    Note that we don't do this to "GOAL" files.  The executables don't need
#    this.
#
# SO_GOALS := $(strip $(addsuffix .so.$(PATCH_SFX), $(basename $(SO_GOALS))))
# AR_GOALS := $(strip $(addsuffix .a.$(PATCH_SFX),  $(basename $(AR_GOALS))))

SO_GOALS := $(strip $(addsuffix .so, $(basename $(SO_GOALS))))
AR_GOALS := $(strip $(addsuffix .a,  $(basename $(AR_GOALS))))

#
#    Define the top level library directory.  This is where all of the runtime
#    shared object libraries will be installed.  This directory will be
#    created if it does not already exist.
#
TOP_LIBRARY = $(TOP)/lib
TOP_LIB = $(TOP_LIBRARY)

#
#    Define the top level binary directory.  This is where all of the
#    executables will be installed.  This directory will be created if it does
#    not already exist.
#
TOP_BINARY = $(TOP)/bin
TOP_BIN = $(TOP_BINARY)

#
#    Define the top level include directory.  This is where all of the
#    includes will be installed.  This directory will be created if it does
#    not already exist.
#
TOP_INCLUDE = $(TOP)/include
TOP_INC = $(TOP_INCLUDE)

#
#    Define the top level documentation directory.  This is where all of the
#    doxygen generated documentation will be installed as well as any other
#    document files.
#
TOP_DOC = $(TOP)/doc

#
#    Define the top level required directories.
#
TOP_LEVEL_DIRS = $(TOP_LIBRARY) $(TOP_BINARY) $(TOP_INCLUDE) $(TOP_DOC)

#
#    Define the macros to install files into the binary and library
#    directories.
#
#    Note: The first macro of each pair is the "normal" one to use.  The macro
#    with the word "PLAIN" in it is the same as the "normal" one but without
#    execute permissions on the installed file.  The "normal" mode commands
#    will install files with the execute permissions flag set unless this
#    doesn't make sense (such as the case of "config" files).
#
INSTALL_DIR           = install -m 777 -d
INSTALL_CMD           = install -p -m 777 -t
INSTALL_PLAIN_CMD     = install -p -m 666 -t

INSTALL_BIN           = $(INSTALL_CMD) $(TOP_BINARY)
INSTALL_PLAIN_BIN     = $(INSTALL_PLAIN_CMD) $(TOP_BINARY)

INSTALL_LIB           = $(INSTALL_CMD) $(TOP_LIBRARY)
INSTALL_PLAIN_LIB     = $(INSTALL_PLAIN_CMD) $(TOP_LIBRARY)

INSTALL_INCLUDE       = $(INSTALL_CMD) $(TOP_INCLUDE)
INSTALL_PLAIN_INCLUDE = $(INSTALL_PLAIN_CMD) $(TOP_INCLUDE)

#
#    Define the files that the "clean" rule will delete.
#
# CLEAN_FILES = *$(VERSION_FILE).* \
# 	      $(OBJ_DIR)*$(VERSION_FILE).* \
#
CLEAN_FILES = $(ALL_GOALS) \
				$(ALL_OBJS) \
				$(OBJ_DIR)*.log \
				*.d \
				*~

#
#    Figure out if this is a 64 bit platform.
#
#    32 bit platforms will have "i386" here.
#    64 bit platforms will have "x86_64" here.
#
PLATFORM := $(shell uname -i)

#
#    Define the global optimization flag.  Normally the same optimization
#    level will be used for all compilations and links.  If this is the
#    desired behavior, simply redefine the following variable.  If different
#    flags are needed for each type of operation, the individual optimization
#    flags defined below can be set as needed.
#
OPTIMIZATION = -g

COMMON_INCLUDES = $(INCLUDE_DIRS)

#
#    Defines needed for building the executables
#
#   DEFINE             PURPOSE
#-----------------------------------------------------------------------
# _FILE_OFFSET_BITS=64 Needed by all that want stat(2) to not fail with
#                      EOVERFLOW
#
DEFINES=-D_FILE_OFFSET_BITS=64

#
#    Set the appropriate flags for C and C++ compilations in 64 and 32 bit
#    environments.
#
ifeq "$(PLATFORM)" "x86_64"
    CC_OPTS  := -fPIC
    CXX_OPTS := -fPIC
else
    CC_OPTS  :=
    CXX_OPTS :=
endif

#
#    Define the flags and commands to compile a C file.
#
CC              ?= gcc
CC_OPTIMIZATION = $(OPTIMIZATION)
CC_INCLUDES     = $(COMMON_INCLUDES)
CC_OPTS        += -Wall
override CC_FLAGS       += $(CC_OPTIMIZATION) $(CC_INCLUDES) $(CC_OPTS) $(DEFINES)
CC_CMD          = $(CC) $(CC_FLAGS)

#
#    Define the flags and commands to compile a C++ file.
#
CXX              ?= g++
CXX_OPTIMIZATION = $(OPTIMIZATION)
CXX_INCLUDES     = $(COMMON_INCLUDES)
CXX_OPTS        += -Wall
override CXX_FLAGS       += $(CXX_OPTIMIZATION) $(CXX_INCLUDES) $(CXX_OPTS) $(DEFINES)
CXX_CMD          = $(CXX) $(CXX_FLAGS)

#
#    Define the flags and commands to link C++ files.
#
LD               ?= gcc
LD_OPTIMIZATION  = $(OPTIMIZATION)
override LD_FLAGS += $(LD_OPTIMIZATION)
LD_LIB_DIRS     += -L. -L..
LD_LIB_DIRS     += -L$(TOP_LIBRARY)

ifneq "$(OBJ_DIR)" ""
    LD_LIB_DIRS += -L$(OBJ_DIR)
endif

LD_LIBS += -lc
LD_OPTS  = $(LD_FLAGS) $(LD_LIB_DIRS)
LD_CMD	 = $(LD) $(LD_OPTS)

SO_CMD   = $(LD_CMD) -shared -fPIC
AR_CMD   ?= ar

#
#    If the user has not disabled the automatic installation feature, add the
#    install rule to the list of default goals.
#
#    Note that if the user has disabled the automatic installation feature by
#    setting the NO_AUTO_INSTALL variable, installation can be performed by
#    typing: "make install".
#
ifndef NO_AUTO_INSTALL
    AUTO_INSTALL = install
endif

ifndef NO_AUTO_SUBDIRS
    AUTO_SUBDIRS = subdirs
endif

#
#    If the user asked for no directory name output, just do the makes in the
#    subdirectories silently.
#
ifdef NO_DIR_OUTPUT
    DIR_ECHO := \#
else
    DIR_ECHO := echo
endif

#
#    This is the default target for this makefile.  To invoke this rule:
#
#        make
#
#    or:
#
#        make all
#
#    It will recurse down into any subdirectories that the user may have
#    specified and then perform the build of the "goal" file in the current
#    directory.
#
GOAL_LIST   = $(strip $(SO_GOALS) $(AR_GOALS) $(GOALS) $(DEFAULT_GOALS) \
                      $(AUTO_INSTALL))

DIRS_NEEDED = $(strip $(TOP_LEVEL_DIRS) $(OBJ_DIR) $(DEPS_DIR))


ALL_TARGETS = $(filter-out ./ .,$(DIRS_NEEDED) $(GOAL_LIST) $(AUTO_SUBDIRS))

.PHONY: all
all: $(ALL_TARGETS)

$(SUBDIRS): $(GOAL_LIST)

#
#    The following targets will build all of the directories specified as
#    "SUBDIRS" by the user.  If no subdirectories were specified this will not
#    do anything.
#
#    Note that the directory names that are displayed on entry/exit are the
#    tail of the current directory path with the "TOP" portion of the path
#    removed.  That makes all the displayed directories relative paths from
#    the "TOP" which is much less cluttered and easier to read.
#
.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	curdir=$(subst $(TOP)/,,$(CURDIR)/$@); \
	$(DIR_ECHO) "===> [$(MAKELEVEL)] Moving into $$curdir ..."; \
	cd $@; \
	$(MAKE) MAKEFLAGS=$(MAKEFLAGS) $(MAKECMDGOALS); \
	if [ $$? -ne 0 ]; \
	then \
	    exit 255; \
	fi; \
	cd ..; \
	$(DIR_ECHO) "<=== [$(MAKELEVEL)] Moving out of $$curdir"

#
#    This rule will generate all of the executable goal files.  To invoke this
#    rule, simply type "make" or "make all".  This rule is used to generate all
#    of the "normal" executable files.
#
define GOAL_TEMPLATE

    OBJS_$(1) := $(addprefix $(OBJ_DIR), $(OBJS_$(1)))

    $(2): $$(OBJS_$(1))
	echo "Linking executable file $(1)"; \
	$$(LD_CMD) $$(LD_OPTS_$(1)) -o $(2) $$(OBJS_$(1)) \
	    $$(LD_LIBS_$(1)) $$(LD_LIBS); \

    ALL_OBJS  += $$(OBJS_$(1))
    ALL_GOALS += $(2)

endef

#
#    For each executable specified in the "GOALS" variable, go generate an
#    instance of the GOAL_TEMPLATE macro and then evaluate it as a target
#    rule.
#
$(foreach p,$(GOALS),$(eval $(call GOAL_TEMPLATE,$(strip $(subst \
    $(VERSION_SUFFIX),,$(notdir $(p)))),$(p))))

#
#    This rule will generate all of the shared library goal files.  To invoke
#    this rule, simply type "make" or "make all".
#
#    Input Parameters:
#
#     $1 - The base name of the target. e.g. "libsomeLibrary"
#     $2 - The path name of the target. e.g. "./obj/libsomeLibrary.so.1.2.1"
#
#    For the above parameters, the derived values might be:
#
#      OBJS_libsomeLibrary: "obj/service.o obj/application.o"
#      VER_libsomeLibrary:  "obj/libsomeLibrary_version_"
#
#    Note that the symbolic link that is created at the end of this rule is
#    created strictly to make linking with a library IN THIS DIRECTORY easier.
#    Normally this only happens when the makefile contains rules that link
#    with a library that is also being created by this same make rule.  For
#    instance, the makefile builds a shared library and then builds an
#    executable that links with that same shared library.
#
#
#    This is being done primarily to prevent the need for coding a pretty ugly
#    "SO_LIBS_..." command.  For an executable named gorph that is being built
#    with a library called snitz that was also built by this makefile, the
#    library directive might look something like:
#
#        SO_LIBS_gorph += -lsomeLibrary \
#                         $(OBJ_DIR)libsnitz.so.$(PATCH_SFX)
#
#    By creating the symbolic link for the libsnitz.so library, we don't need
#    to supply the version number above and it becomes:
#
#        SO_LIBS_gorph += -lsomeLibrary -lsnitz
#
#    That's much cleaner and less error prone!
#
#    Note that this is only an issue for libraries that are being built in the
#    same directory as something else that links with it.  For libraries that
#    have been built in other directories, it is not an issue because those
#    libraries have already been "installed" in the $(TOP)/lib directory with
#    all of the appropriate symlinks and the make system knows to look for
#    them there.  Also note that the current directory and the OBJ_DIR
#    directory (if it is defined) are always included in the "-L" linker
#    directives automatically by the make system.
#
define SO_GOAL_TEMPLATE

    OBJS_$(1) := $(addprefix $(OBJ_DIR), $(OBJS_$(1)))

    $(2): $$(OBJS_$(1))
	echo "Linking shared object file $(2)"; \
	$$(SO_CMD) $$(SO_OPTS_$(1)) -o $(2) \
		$$(OBJS_$(1)) $$(SO_LIBS_$(1)) $$(SO_LIBS); \
	if [ -n "$(OBJ_DIR)" ]; \
	then \
	    cd $(OBJ_DIR); \
	fi;

    ALL_OBJS  += $$(OBJS_$(1))
    ALL_GOALS += $(2)

endef

#
#    For each executable specified in the "SO_GOALS" variable, go generate an
#    instance of the SO_GOAL_TEMPLATE macro and then evaluate it as a target
#    rule.
#
$(foreach p,$(SO_GOALS),$(eval $(call SO_GOAL_TEMPLATE,$(strip $(subst \
    .so,,$(notdir $(p)))),$(p))))

#
#    This rule will generate all of the archive library goal files.  To invoke
#    this rule, simply type "make" or "make all".
#
define AR_GOAL_TEMPLATE

    OBJS_$(1) := $(addprefix $(OBJ_DIR), $(OBJS_$(1)))

    $(2): $$(OBJS_$(1))
	echo "Linking archive file $(2)"; \
	$$(AR_CMD) $$(AR_OPTS_$(1)) r $(2) $$(OBJS_$(1)) \
	     $$(AR_LIBS_$(1)); \

    ALL_OBJS  += $$(OBJS_$(1))
    ALL_GOALS += $(2)

endef

#
#    For each executable specified in the "AR_GOALS" variable, go generate an
#    instance of the AR_GOAL_TEMPLATE macro and then evaluate it as a target
#    rule.
#
$(foreach p,$(AR_GOALS),$(eval $(call AR_GOAL_TEMPLATE,$(strip $(subst \
    .a,,$(notdir $(p)))),$(p))))


ifdef ENABLE_VERSIONING

#
#    This rule will generate the version string for the current goal.  The
#    version string will be generated into a C source file and that file
#    will be compiled and linked into the final executable.
#
#    The version string will look something like:
#
#        CODE_VER: parseTest 1.2.3 built Tue Jan  6 13:51:01 PST 2007 by dmies \
#                 on hostMachineName
#
#    This string can be displayed in the final executable by doing something
#    like:
#
#        strings parseTest | grep CODE_VER
#
$(OBJ_DIR)%$(VERSION_FILE).o:
	echo "Generating version file $(@:%.o=%.c)";
	echo $(VERSION_STRING) > $(@:%.o=%.c); \
	$(CC_CMD) $(@:%.o=%.c) -o $@

endif

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
#    Other examples of install rules might be:
#
#        INSTALL_BIN_FILES = $(GOALS)
#        INSTALL_LIB_FILES = $(SO_GOALS) $(AR_GOALS)
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
#    The file lists that are currently available are:
#
#    INSTALL_BIN_FILES       = ""
#    INSTALL_PLAIN_BIN_FILES = ""
#    INSTALL_LIB_FILES       = ""
#    INSTALL_INCLUDE_FILES   = ""
#
.PHONY: install
install: $(SO_GOALS) $(AR_GOALS) $(GOALS) $(DEFAULT_GOALS) $(INSTALL_DEPS)
	if [ -n "$(INSTALL_BIN_FILES)" ]; \
	then \
            $(INSTALL_BIN) $(INSTALL_BIN_FILES); \
		fi; \
	\
	\
	\
	if [ -n "$(INSTALL_PLAIN_BIN_FILES)" ]; \
	then \
            $(INSTALL_PLAIN_BIN) $(INSTALL_PLAIN_BIN_FILES); \
		fi; \
	\
	\
	\
	if [ -n "$(INSTALL_LIB_FILES)" ]; \
	then \
	    for file in $(INSTALL_LIB_FILES); \
	    do \
		src=$$file; \
	        if [ -n "$(OBJ_DIR)" -a \
		     $$( expr $$file : '.*$(OBJ_DIR)' ) -eq 0 ]; \
		then \
		    src=$(OBJ_DIR)$$file; \
		fi; \
	        if [ $$( expr $$src : '.*' ) -eq 0 ]; \
		then \
		    src=$$src; \
		fi; \
		$(INSTALL_LIB) $$src; \
	    done; \
	fi; \
	\
	\
	\
	if [ -n "$(INSTALL_INCLUDE_FILES)" ]; \
	then \
	    for file in $(INSTALL_INCLUDE_FILES); \
	    do \
			$(INSTALL_INCLUDE) $(OBJ_DIR)$$file; \
	    done; \
	fi; \
	\
	\
	\
	if [ -n "$(INSTALL_PLAIN_INCLUDE_FILES)" ]; \
	then \
		$(INSTALL_PLAIN_INCLUDE) $(INSTALL_PLAIN_INCLUDE_FILES); \
	fi; \

#
#    This rule will remove all generated files from the current directory.  To
#    invoke this rule:
#
#        make clean
#
#    This rule will not remove the dependencies or dependency directory nor
#    will it remove any cache directories in the object directory or the
#    object directory itself.
#
.PHONY: clean
clean: DIR_ECHO := ":"
clean: cleanProlog subdirs cleanEpilog
	@$(DIR_ECHO) "Removing all generated files"; \
	 rm -rf $(CLEAN_FILES)


.PHONY: cleanProlog
cleanProlog:
	@if [ "$(MAKELEVEL)" -eq 0 ]; \
	then \
	    echo -n "Removing all generated files... "; \
	fi


.PHONY: cleanEpilog
cleanEpilog:
	@if [ "$(MAKELEVEL)" -eq 0 ]; \
	then \
	    echo "Finished"; \
	fi

#
#    This rule will remove all of the generated files that "make clean" does
#    but it will also remove the dependency files.  Normally we do not want to
#    remove the dependency files since this would cause a complete recompile
#    of the project.
#
#    If we created a directory to store the object files, remove that
#    directory as well as any PureTools cache directories.
#
#    Clients can add items by adding to the variable "DISTCLEAN_FILES" in the
#    same way as the "CLEAN_FILES" is handled.
#
.PHONY: distclean
distclean: DIR_ECHO := ":"
distclean: cleanProlog clean cleanEpilog
	@-if [ "$(DEPS_DIR)" != "." ]; \
	then \
	    rm -rf $(DEPS_DIR); \
	fi; \
	if [ "$(OBJ_DIR)" != "" ]; \
	then \
	    rm -rf $(OBJ_DIR); \
	fi; \
	rm -rf bin/*; \
	rm -rf lib/*; \
	rm -rf include/*; \
	rm -rf $(TOP_BIN)/*; \
	rm -rf $(TOP_LIB)/*; \
	rm -rf $(TOP_INCLUDE)/*; \
	rm -rf $(DISTCLEAN_FILES)

#
#    This rule will invoke the "doxygen" documentation processing system to
#    build a documentation file/files as specified in a configuration file
#    supplied in the "DOXYGEN_FILE" variable.  If this variable is not
#    specified or the value of it is not a readable file, the configuration
#    file we be read from the "build" directory.
#
#    The doxygen template file will be configured by this rule to operate on
#    the files in the current code tree.  The starting point of the file
#    generation will be the directory from which this command was issued thus
#    allowing users to build the documentation in a single directory if
#    desired during the development of new documentation.  This command should
#    be invoked at the top level of the code tree to generate the complete set
#    of documents for the system.
#
#    Note that this rule assumes that the "doxygen" program is installed on
#    the current system and any plugin modules are available.  If the "dot"
#    program is not available, the graphical diagrams will be disabled in the
#    output.
#
.PHONY: doxygen
doxygen:
	 if [ -n "$(DOXYGEN_FILE)" ]; \
	 then \
	     template=$(DOXYGEN_FILE); \
	 else \
	     template=$(TOP)/build/doxygen.template; \
	 fi; \
	 if [ ! -s $$template ]; \
	 then \
	     echo -e "doxygen configuration file \"$$template\"\n" \
		     "   does not exist or is empty - Aborting"; \
	 else \
	     echo "Generating the \"doxygen\" documentation..."; \
	     sed -e 's;!TOP!;$(TOP);g' \
		 -e 's;!BASE_VERSION!;$(BASE_VERSION);g' \
		 $$template > Doxygen; \
	     which dot > /dev/null 2>&1; \
	     if [ $$? -ne 0 ]; \
	     then \
		 echo "HAVE_DOT = NO" >> Doxygen; \
	     fi; \
	     if [ -n "$(DOXYGEN_FILE_SUFFIX)" ]; \
	     then \
		 if [ -s "$(DOXYGEN_FILE_SUFFIX)" ]; \
		 then \
		     cat $(DOXYGEN_FILE_SUFFIX) >> $$template; \
		 fi; \
	     fi; \
	     doxygen Doxygen 2> Doxygen.log 1>/dev/null; \
	     echo ""; \
	     echo "Output from the doxygen run can be found in " \
	         "\"Doxygen.log\""; \
	 fi;

#
#    This is the rule used to compile C files (with a .c extension).
#
#    This rule will also generate the dependency file for each source file
#    compiled.  This process will automatically keep the include dependencies
#    up to date and in sync with the source files.  Note that the dependency
#    files will be kept in the "DEPS_DIR" directory.
#
%.o: %.c
$(OBJ_DIR)%.o : %.c | $(DEPS_DIR) $(OBJ_DIR)
	echo "Compiling C file $<"; \
        $(CC_CMD) -MMD -c $< > $(OBJ_DIR)$*.log 2>&1; \
        if [ $$? -eq 0 ]; \
        then \
	    if [ -s $(OBJ_DIR)$*.log ]; \
	    then \
		echo ''; \
		echo "*** WARNING - See log file $(OBJ_DIR)$*.log" \
		     "- First 20 lines:"; \
	        head -n 20 $(OBJ_DIR)$*.log; \
		echo ''; \
	    else \
		rm -f $(OBJ_DIR)$*.log; \
	    fi; \
        else \
	    echo ''; \
	    echo "*** ERROR - See log file $(OBJ_DIR)$*.log" \
		 "- First 20 lines:"; \
	    head -n 20 $(OBJ_DIR)$*.log; \
	    echo ''; \
	    exit 1; \
        fi; \
	cp $(OBJ_DIR)$*.d $(DEP_ROOT).d; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	    -e '/^$$/ d' -e 's/$$/ :/' < $(OBJ_DIR)$*.d >> $(DEP_ROOT).d; \
	rm -f $(OBJ_DIR)$*.d
#
#    This is the rule used to compile C++ files (with a .cpp or .cxx extension).
#
#    This rule will also generate the dependency file for each source file
#    compiled.  This process will automatically keep the include dependencies
#    up to date and in sync with the source files.  Note that the dependency
#    files will be kept in the "DEPS_DIR" directory.
#
%.o: %.cpp
$(OBJ_DIR)%.o : %.cpp | $(DEPS_DIR) $(OBJ_DIR)
	echo "Compiling C++ file $<"; \
        $(CXX) -MMD $(CXX_FLAGS) -o $@ -c $< > $(OBJ_DIR)$*.log 2>&1; \
        if [ $$? -eq 0 ]; \
        then \
	    if [ -s $(OBJ_DIR)$*.log ]; \
	    then \
		echo ''; \
		echo "*** WARNING - See log file $(OBJ_DIR)$*.log" \
		     "- First 20 lines:"; \
	        head -n 20 $(OBJ_DIR)$*.log; \
		echo ''; \
	    else \
		rm -f $(OBJ_DIR)$*.log; \
	    fi; \
        else \
	    echo ''; \
	    echo "*** ERROR - See log file $(OBJ_DIR)$*.log" \
		 "- First 20 lines:"; \
	    head -n 20 $(OBJ_DIR)$*.log; \
	    echo ''; \
	    exit 1; \
        fi; \
	cp $(OBJ_DIR)$*.d $(DEP_ROOT).d; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	    -e '/^$$/ d' -e 's/$$/ :/' < $(OBJ_DIR)$*.d >> $(DEP_ROOT).d; \
	rm -f $(OBJ_DIR)$*.d

%.o: %.cxx
$(OBJ_DIR)%.o : %.cxx | $(DEPS_DIR) $(OBJ_DIR)
	echo "Compiling C++ file $<"; \
        $(CXX) -MMD $(CXX_FLAGS) -o $@ -c $< > $(OBJ_DIR)$*.log 2>&1; \
        if [ $$? -eq 0 ]; \
        then \
	    if [ -s $(OBJ_DIR)$*.log ]; \
	    then \
		echo ''; \
		echo "*** WARNING - See log file $(OBJ_DIR)$*.log" \
		     "- First 20 lines:"; \
	        head -n 20 $(OBJ_DIR)$*.log; \
		echo ''; \
	    else \
		rm -f $(OBJ_DIR)$*.log; \
	    fi; \
        else \
	    echo ''; \
	    echo "*** ERROR - See log file $(OBJ_DIR)$*.log" \
		 "- First 20 lines:"; \
	    head -n 20 $(OBJ_DIR)$*.log; \
	    echo ''; \
	    exit 1; \
        fi; \
	cp $(OBJ_DIR)$*.d $(DEP_ROOT).d; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	    -e '/^$$/ d' -e 's/$$/ :/' < $(OBJ_DIR)$*.d >> $(DEP_ROOT).d; \
	rm -f $(OBJ_DIR)$*.d

#
#    This rule will create all of the top level directories if they don't
#    already exist
#
$(DIRS_NEEDED):
	@echo "Creating directory $@..."; \
	 install -p -m 777 -d $@; \
	 if [ $$? -ne 0 ]; \
	 then \
	     sudo install -p -m 777 -d $@; \
	 fi
#
#    Include all of the file dependency files here.
#
-include $(wildcard $(DEPS_DIR)/*.d)

#
#    This is a default rule that will propagate any command line arguments
#    down to all of the subdirectories specified, even if there is no target
#    with that name in the current directory.
#
$(MAKECMDGOALS): $(AUTO_SUBDIRS)


##############################################################################
#
#       R u l e :   d e f a u l t
#
#    This is a default rule to catch any typos that users might make.  If the
#    specified target does not exist anywhere else in this file, this rule
#    will be executed.
#
%:
	@echo ""; \
	 echo "Unknown target \"$@\" - Aborting the build"; \
	 echo ""; \
	 exit 2

###############################################################################
#
#	D E B U G G I N G
#
###############################################################################

#
#    Display the flags used to compile and link the code.  Note that this will
#    only be run if the user invokes this target explicitly with
#    "make dumpFlags".
#
.PHONY: dumpFlags
dumpFlags:
	echo ""; \
	echo "Build flag definitions:"; \
	echo ""; \
	echo "            MAKE: $(MAKE)"; \
	echo "       MAKEFLAGS: $(MAKEFLAGS)"; \
	echo "    MAKECMDGOALS: $(MAKECMDGOALS)"; \
	echo ""; \
	echo "     C: $(CC_CMD)"; \
	echo "   C++: $(CXX_CMD)"; \
	echo "    LD: $(LD_CMD)"; \
	echo "    SO: $(SO_CMD)"; \
	echo "    AR: $(AR_CMD)"; \
	echo ""

#
#    The following is for debugging makefiles.
#
#
#    The following make options may be useful to debug makefile problems:
#
#    -d	       Equivalent to "--debug=a" (see below)
#
#    Various levels and types of output can be chosen. With no arguments,
#    print the basic ('b') output. Multiple values must be comma- or
#    space-separated.
#
#    --debug=a (all) - All types of debugging output are enabled. This is
#              equivalent to using `-d'.
#
#    --debug=b (basic) - Basic debugging prints each target that was found to
#              be out-of-date, and whether the build was successful or not.
#
#    --debug=v (verbose) - A level above `basic'; includes messages about
#              which makefiles were parsed, prerequisites that did not need to
#              be rebuilt, etc. This option also enables `basic' messages.
#
#    --debug=i (implicit) - Prints messages describing the implicit rule
#              searches for each target. This option also enables `basic'
#              messages.
#
#    --debug=j (jobs) - Prints messages giving details on the invocation of
#              specific subcommands.
#
#    --debug=m (makefile) - By default, the above messages are not enabled
#              while trying to remake the makefiles. This option enables
#              messages while rebuilding makefiles, too.  Note that the `all'
#              option does enable this option. This option also enables
#              `basic' messages.
#
#    -p        This make option will list (to stdout) the entire makefile
#              after all includes and variable substitutions have taken place.
#              This can be very useful in figuring out why a rule does not
#              work as expected.
#

#
#    To display the value of any variable or macro, simply type:
#
#        make print-X (where "X" is the name of the variable/macro)
#
#    For example, to display the value of the "GOALS" variable:
#
#        make print-GOALS
#
#    If the variable you are interested in is defined differently for
#    different targets, specify the target on the make line:
#
#        make all print-GOALS
#
print-%:
	@$(info )
	@$(info $*="$($*)")
	@$(info )
	@$(info Defined in $(origin $*) as: "$(value $*)")
	@$(error )

$(filter-out print-%, $(MAKECMDGOALS)): $(filter print-%, $(MAKECMDGOALS))

#
#    Enable command output in the makefile.  Defining the variable
#    "TRACE_COMMANDS" (either in this makefile or in the user's customized
#    makefile) will cause each shell command to be echoed to stdout just
#    before it is executed.  The commands will be echoed after all variable
#    substitution has been performed and will look exactly like what the shell
#    will receive as input.  This feature can be used to check that all
#    command line switches and options are being set as expected.  This
#    operates much like the -n option to make except that defining this
#    variable will display the shell command and then execute it whereas the
#    "-n" option to make will only display the commands without executing
#    them.
#
#    To enable this feature, add a line like the following to your makefile:
#
#        TRACE_COMMANDS = 1
#
#    You can also enable this feature without modifying any makefiles by
#    setting the variable on the make command line (note the lack of spaces
#    around the equal sign!):
#
#        make TRACE_COMMANDS=1
#
ifndef TRACE_COMMANDS
    MAKEFLAGS := $(MAKEFLAGS)s
endif

#
#    The user may want to disable the parallel build feature to see exactly
#    what order the makefile will build in.
#
#    The user can disable parallel builds by defining the NO_PARALLEL_BUILDS
#    variable:
#
#        NO_PARALLEL_BUILDS = 1
#
#    The user MUST define this before he includes the master makefile in order
#    for it to take effect.
#
ifndef NO_PARALLEL_BUILDS
    MAKEFLAGS := $(MAKEFLAGS)j
endif

#
#    Enable rule tracing in the makefile.  Defining the variable
#    "TRACE_RULES" (either in this makefile or in the user's master makefile)
#    will cause output to be generated each time make is about to execute a
#    rule.  The output will give the target being built, the complete list of
#    prerequisites that the target depends on, and the list of prerequisites
#    that need to be regenerated before the target can be built.
#
#    To enable this feature, add a line like the following to your makefile:
#
#        TRACE_RULES = 1
#
#    You can also enable this feature without modifying any makefiles by
#    setting the variable on the make command line (note the lack of spaces
#    around the equal sign!):
#
#        make TRACE_RULES=1
#
ifdef TRACE_RULES
    OLD_SHELL := $(SHELL)
    SHELL = $(info RULE: $@ Depends On: $^ Newer Files: $?)$(OLD_SHELL)
endif
