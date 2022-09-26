## ======================================================================
## partial Makefile provided to students
##
LIBMONGOOSEDIR = libmongoose/

## don't forget to export LD_LIBRARY_PATH pointing to it

VIPS_CFLAGS += $$(pkg-config vips --cflags)

VIPS_LIBS   += $$(pkg-config vips --libs) -lm

.PHONY: clean new newlibs style \
feedback feedback-VM-CO clone-ssh clean-fake-ssh \
submit1 submit2 submit

CFLAGS += -std=c11 -Wall -pedantic -g #-fsanitize=address
#LDFLAGS += -fsanitize=address

# a bit more checks if you'd like to (uncomment)
# CFLAGS += -Wextra -Wfloat-equal -Wshadow                         \
# -Wpointer-arith -Wbad-function-cast -Wcast-align -Wwrite-strings \
# -Wconversion -Wunreachable-code

# ----------------------------------------------------------------------
# feel free to update/modifiy this part as you wish

TARGETS := lib imgStoreMgr imgStore_server

all: $(TARGETS)
CHECK_TARGETS := tests/test-imgStore-implementation
CHECK_TARGETS += tests/unit-test-cmd_args
CHECK_TARGETS += tests/unit-test-dedup
OBJS := error.o imgst_list.o tools.o util.o imgst_create.o imgst_delete.o image_content.o dedup.o imgst_insert.o imgst_read.o imgst_gbcollect.o
RUBS = $(OBJS) core

lib: $(LIBMONGOOSEDIR)/libmongoose.so

$(LIBMONGOOSEDIR)/libmongoose.so: $(LIBMONGOOSEDIR)/mongoose.c  $(LIBMONGOOSEDIR)/mongoose.h
	make -C $(LIBMONGOOSEDIR)

imgStoreMgr: imgStoreMgr.o $(OBJS) 
imgStoreMgr: LDLIBS += $(VIPS_LIBS) -lssl -lcrypto -ljson-c

imgStore_server: lib imgStore_server.o $(OBJS)
imgStore_server: 
	gcc -o imgStore_server imgStore_server.o $(OBJS) $(VIPS_LIBS) -lssl -lcrypto -L libmongoose -lmongoose -ljson-c


imgStoreMgr.o: CFLAGS += $(VIPS_CFLAGS) 
imgStore_server.o: CFLAGS += $(VIPS_CFLAGS) -I libmongoose
image_content.o: CFLAGS += $(VIPS_CFLAGS) 

tests/unit-test-cmd_args.o: tests/unit-test-cmd_args.c tests/tests.h \
  error.h imgStore.h
  
tests/unit-test-cmd_args: tests/unit-test-cmd_args.o $(OBJS)

tests/unit-test-dedup.o: tests/unit-test-dedup.c tests/tests.h error.h imgStore.h dedup.h

tests/unit-test-dedup: tests/unit-test-dedup.o $(OBJS)

# ----------------------------------------------------------------------
# This part is to make your life easier. See handouts how to make use of it.
## ======================================================================
## Tests

# target to run black-box tests
check:: all
	@if ls tests/*.*.sh 1> /dev/null 2>&1; then \
	    for file in tests/*.*.sh; do [ -x $$file ] || echo "Launching $$file"; ./$$file || exit 1; done; \
	fi

# all those libs are required on Debian, adapt to your box
$(CHECK_TARGETS): LDLIBS += -lcheck -lm -lrt -pthread -lsubunit $(VIPS_LIBS) -lssl -lcrypto -ljson-c

check:: CFLAGS += -I. $(VIPS_CFLAGS)

check:: $(CHECK_TARGETS)
	export LD_LIBRARY_PATH=.; $(foreach target,$(CHECK_TARGETS),./$(target) &&) true
  
clean::
	-@/bin/rm -f *.o *~ $(CHECK_TARGETS)

new: clean all

static-check:
	CCC_CC=$(CC) scan-build -analyze-headers --status-bugs -maxloop 64 make -j1 new

style:
	astyle -n -o -A8 -xt0 *.[ch]

## ======================================================================
## Feedback

IMAGE=chappeli/pps21-feedback:week13
## Note: vous pouvez changer le tag latest pour week04, ou week05, etc.

REPO := $(shell git config --get remote.origin.url)
SSH_DIR := $(HOME)/.ssh

feedback:
	@echo Will use $(REPO) inside container
	@docker pull $(IMAGE)
	@docker run -it --rm -e REPO=$(REPO) -v $(SSH_DIR):/opt/.ssh $(IMAGE)

clone-ssh:
	@-$(eval SSH_DIR := $(HOME)/.$(shell date "+%s;$$"|sha256sum|cut -c-32))
	@cp -r $(HOME)/.ssh/. $(SSH_DIR)

clean-fake-ssh:
	@case $(SSH_DIR) in $(HOME)/\.????????????????????????????????) $(RM) -fr $(SSH_DIR) ;; *) echo "Dare not remove \"$(SSH_DIR)\"" ;; esac

feedback-VM-CO: clone-ssh feedback clean-fake-ssh

## ======================================================================
## Submit

SUBMIT_SCRIPT=../provided/submit.sh
submit1: $(SUBMIT_SCRIPT)
	@$(SUBMIT_SCRIPT) 1

submit2: $(SUBMIT_SCRIPT)
	@$(SUBMIT_SCRIPT) 2

submit:
	@printf 'what "make submit"??\nIt'\''s either "make submit1" or "make submit2"...\n'