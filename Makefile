# makefile for swatdb buffer manager lab assignment
#
# make:          builds sandbox and all unit tests
# make clean:    cleans up compiled files
# make runtests: runs all unit tests
#

# location of make.config
SWATCONFIGDIR = ./
# defines SWATDBDIR
include $(SWATCONFIGDIR)/make.config

# path to SwatDB library files
LIBDIR = $(SWATDB_DIR)/lib/

# paths to include directories
INCLUDES = -I. -I$(SWATDB_DIR)/include/

# compiler
CC = g++


# compiler flags for bufmgr assignment solution
# (define BUFMGR_LAB_SOL_OMIT)
CFLAGS =  -g -Wall -DBUFMGR_LAB_SOL_OMIT=1

# lflags for linking
LFLAGS =  -L$(LIBDIR)

# SwatDB libraries needed to link in to buf manager test
LIBS = $(LFLAGS) -l swatdb 

SRCS = bufmgr.cpp bm_buffermap.cpp bm_frame.cpp bm_replacement.cpp  bm_policies.cpp

# suffix replacement rule
OBJS = $(SRCS:.cpp=.o)

# be very careful to not add any spaces to ends of these
TARGET = sandbox
CHKPT = checkpt
UNITTESTS = unittests
REPTESTS = replacementtests
CLOCKTESTS = clocktests
PERFTESTS = performancetests

# generic makefile
.PHONY: clean

all: $(TARGET) $(UNITTESTS) $(CHKPT) $(REPTESTS) $(CLOCKTESTS) $(PERFTESTS)

$(TARGET): $(OBJS) $(TARGET).cpp *.h
	@echo "swat_db_dir" $(SWATDB_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(TARGET).cpp $(OBJS) $(LIBS)

$(CHKPT): $(OBJS) $(CHKPT).cpp *.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $(CHKPT) $(CHKPT).cpp  -lUnitTest++ $(OBJS) $(LIBS)

$(UNITTESTS): $(OBJS) $(UNITTESTS).cpp *.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $(UNITTESTS) $(UNITTESTS).cpp  -lUnitTest++ $(OBJS) $(LIBS)

$(REPTESTS): $(OBJS) $(REPTESTS).cpp *.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $(REPTESTS) $(REPTESTS).cpp  -lUnitTest++ $(OBJS) $(LIBS)

$(CLOCKTESTS): $(OBJS) $(CLOCKTESTS).cpp *.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $(CLOCKTESTS) $(CLOCKTESTS).cpp  -lUnitTest++ $(OBJS) $(LIBS)


$(PERFTESTS): $(OBJS) $(PERFTESTS).cpp *.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $(PERFTESTS) $(PERFTESTS).cpp  -lUnitTest++ $(OBJS) $(LIBS)

# suffix replacement rule using autmatic variables:
# automatic variables: $< is the name of the prerequiste of the rule
# (.cpp file),  and $@ is name of target of the rule (.o file)
.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

runtests: $(UNITTESTS) $(CHKPT) $(CLOCKTESTS) $(REPTESTS) $(PERFTESTS)
	@echo "---------checkpoint---------" 
	sleep 2
	./$(CHKPT)
	sleep 2
	@echo "---------unittests---------" 
	sleep 2
	./$(UNITTESTS)
	sleep 2
	@echo "---------clocktests---------" 
	sleep 2
	./$(CLOCKTESTS)
	sleep 2
	@echo "---------replacetests---------" 
	sleep 2
	./$(REPTESTS)
	sleep 2
	@echo "---------perftests---------" 
	sleep 2
	./$(PERFTESTS)

clean:
	$(RM) *.o $(TARGET) $(UNITTESTS) $(CHKPT) $(REPTESTS) $(CLOCKTESTS) $(PERFTESTS)
	chmod 744 cleanup.sh
	./cleanup.sh
