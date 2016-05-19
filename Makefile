#
#   Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.
#
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this file,
#   You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
#	Top Level Vehicle Signal Interface build rules.
#
.SILENT:

SUBDIRS = core api

TARGETS = all docs clean distclean
$(TARGETS): subdirs

.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)

$(SUBDIRS):
	curdir=$(shell pwd)/$@; \
    echo "===> Building in $$curdir ..."; \
    $(MAKE) MAKEFLAGS=$(MAKEFLAGS) -C $@ $(MAKECMDGOALS); \
    echo "<=== ...Finished building in $$curdir"

