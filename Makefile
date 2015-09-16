CC=g++
MBCC=microblazeel-xilinx-linux-gnu-g++
BEAGLECC=arm-linux-gnueabi-g++

#
#++++++++++++++++++++
# Paths
#--------------------
CPPUTEST_HOME = ../CppUTest
SPACE_LIB = ../space-lib
SPACE_UTLS = $(SPACE_LIB)/utls
SPACE_UPTDATER = ../space-updater
SPACE_SCRIPT = ../space-script

COMMON_BIN = bin/common
COMMON_Q6_BIN = bin/commonQ6
SPACE_COMMANDER_BIN = bin/space-commander
SPACE_COMMANDER_Q6_BIN = bin/space-commanderQ6
GROUND_COMMANDER_BIN = bin/ground-commander

#
#++++++++++++++++++++
# Flags
#--------------------
CFLAGS += -Wall
CPPFLAGS += -Wall -I$(CPPUTEST_HOME)/include
MEM_LEAK_MACRO = -include $(CPPUTEST_HOME)/include/CppUTest/MemoryLeakDetectorMallocMacros.h  -include $(CPPUTEST_HOME)/include/CppUTest/MemoryLeakDetectorNewMacros.h
MICROCFLAGS=-mcpu=v8.40.b -mxl-barrel-shift -mxl-multiply-high -mxl-pattern-compare -mno-xl-soft-mul -mno-xl-soft-div -mxl-float-sqrt -mhard-float -mxl-float-convert -mlittle-endian -Wall
DEBUGFLAGS=-ggdb -g -gdwarf-2 -g3 #gdwarf-2 + g3 provides macro info to gdb

#
#++++++++++++++++++++
# includes
#--------------------
INCLUDES = -I./include/ -I./include/ground-commander/ -I$(SPACE_LIB)/include -I$(SPACE_UPTDATER)/include -I$(SPACE_SCRIPT)/tgz-wizard/include -I$(SPACE_UTLS)/include -I$(SPACE_LIB)/shakespeare/inc
INCTESTPATH = -I./tests/unit/stubs/ -I./tests/helpers/include/

#
#++++++++++++++++++++
# Libraries
#--------------------
LIBPATH=-L./lib/ -L$(SPACE_LIB)/shakespeare/lib -L$(CPPUTEST_HOME)/lib -L$(SPACE_UTLS)/lib

make_dir:
	mkdir -p bin && mkdir -p $(COMMON_BIN) && mkdir -p $(SPACE_COMMANDER_BIN) && mkdir -p $(GROUND_COMMANDER_BIN) && mkdir -p $(SPACE_COMMANDER_Q6_BIN) && mkdir -p $(COMMON_Q6_BIN) && mkdir -p lib

#
#++++++++++++++++++++
# 	CppUTest / PC
#--------------------
LIBS=-lshakespeare -lcs1_utls
CPPUTEST_LIBS=-lCppUTest -lCppUTestExt 

#
# All Object files, do not use wildcard, add the ones you need explicitly!
#
COMMON_OBJECTS = $(COMMON_BIN)/subsystems.o $(COMMON_BIN)/command-factory.o $(COMMON_BIN)/deletelog-command.o  $(COMMON_BIN)/decode-command.o $(COMMON_BIN)/getlog-command.o $(COMMON_BIN)/gettime-command.o $(COMMON_BIN)/reboot-command.o $(COMMON_BIN)/settime-command.o $(COMMON_BIN)/update-command.o 

OBJECTS = $(SPACE_COMMANDER_BIN)/Net2Com.o $(SPACE_COMMANDER_BIN)/NamedPipe.o $(SPACE_COMMANDER_BIN)/base64.o

#
# CppUTest files, no wildcard, add files explicitly!
#
UNIT_TEST = tests/unit/Net2Com-test.cpp  tests/unit/deletelog-command-test.cpp  tests/unit/getlog-command-test.cpp tests/unit/commander-test.cpp tests/unit/settime-command-test.cpp  tests/unit/gettime-command-test.cpp tests/unit/ground-commander-test.cpp
CS1_UTEST_DIR="cs1_utest" # as defined in SpaceDecl.h

#
# ENV : either CS1_UTEST for test environment or empty for PROD, perform a 'make clean' when changing this parameter
#
UTEST_ENV=-DCS1_UTEST $(MEM_LEAK_MACRO) $(CPPUTEST_LIBS) 
ENV = -DCS1_DEBUG -DGROUND_MOCK_SAT -DPRESERVE

#
#++++++++++++++++++++
# Common
#--------------------
bin/common/%.o: src/common/%.cpp include/common/%.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(DEBUGFLAGS) $(INCLUDES) -c $< -o $@ $(ENV) 

#
#++++++++++++++++++++
# CppUTest / PC Rules
#--------------------
buildBin: make_dir $(SPACE_COMMANDER_BIN) staticlibs.tar buildGroundCommander

$(SPACE_COMMANDER_BIN)/%.o: src/space-commander/%.cpp include/space-commander/%.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(DEBUGFLAGS) $(INCLUDES) -c $< -o $@ $(ENV) 

$(SPACE_COMMANDER_BIN): src/space-commander/space-commander-main.cpp $(COMMON_OBJECTS) $(OBJECTS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(DEBUGFLAGS) $(INCLUDES) $(LIBPATH) -o $@/space-commander $^ $(LIBS) $(ENV)

test: ENV = -DCS1_DEBUG  $(UTEST_ENV)  -DPRESERVE -DGROUND_MOCK_SAT
test: buildBin make_dir bin/AllTests $(SPACE_COMMANDER_BIN)
	mkdir -p $(CS1_UTEST_DIR)

bin/AllTests: tests/unit/AllTests.cpp  $(UNIT_TEST) $(COMMON_OBJECTS) $(OBJECTS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(DEBUGFLAGS) $(INCLUDES) $(LIBPATH) -o $@ $^ $(LIBS) $(ENV)
	
#
#++++++++++++++++++++
# Ground Commander
#--------------------

# NOTE, GROUND COMMANDER IS CURRENTLY BUILDING IN DEMO MODE, SO NAMED PIPES ARE PLACED 
# IN /home/pipes/ground/ RATHER THAN /home/pipes, SO THAT BOTH THE GROUND AND SATELLITE
# COMMANDERS CAN RUN SIMULTANEOUSLY
# TODO - DISABLE THE GROUND_MOCK_SAT FLAG WHEN THIS IS NOT NEEDED ANYMORE

buildGroundCommander: make_dir $(GROUND_COMMANDER_BIN) staticlibs.tar

$(GROUND_COMMANDER_BIN): src/ground-commander/ground-commander-main.cpp $(COMMON_OBJECTS) $(OBJECTS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(DEBUGFLAGS) $(INCLUDES) $(LIBPATH) -o $@/ground-commander $^ $(LIBS) $(ENV) 

#
#++++++++++++++++++++
#  MicroBlaze 
#--------------------
LIBS_Q6= -lshakespeare-mbcc -lcs1_utlsQ6

COMMON_Q6_OBJECTS = $(COMMON_Q6_BIN)/command-factoryQ6.o $(COMMON_Q6_BIN)/deletelog-commandQ6.o $(COMMON_Q6_BIN)/decode-commandQ6.o $(COMMON_Q6_BIN)/getlog-commandQ6.o $(COMMON_Q6_BIN)/gettime-commandQ6.o $(COMMON_Q6_BIN)/reboot-commandQ6.o $(COMMON_Q6_BIN)/settime-commandQ6.o $(COMMON_Q6_BIN)/update-commandQ6.o $(COMMON_Q6_BIN)/subsystemsQ6.o

 

OBJECTS_Q6 = $(SPACE_COMMANDER_Q6_BIN)/Net2ComQ6.o $(SPACE_COMMANDER_Q6_BIN)/NamedPipeQ6.o $(SPACE_COMMANDER_Q6_BIN)/base64Q6.o 

#
#++++++++++++++++++++
# Common Q6
#--------------------
bin/commonQ6/%Q6.o: src/common/%.cpp include/common/%.h
	$(MBCC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(DEBUGFLAGS) $(INCLUDES) -c $< -o $@ 
	
#
#++++++++++++++++++++
# MicroBlaze Rules 
#--------------------
buildQ6:  make_dir $(SPACE_COMMANDER_Q6_BIN) staticlibsQ6.tar buildGroundCommander
	
$(SPACE_COMMANDER_Q6_BIN)/%Q6.o: src/space-commander/%.cpp include/space-commander/%.h
	$(MBCC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(DEBUGFLAGS) $(INCLUDES) -c $< -o $@

$(SPACE_COMMANDER_Q6_BIN): src/space-commander/space-commander-main.cpp $(COMMON_Q6_OBJECTS) $(OBJECTS_Q6)
	$(MBCC) $(CFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(DEBUGFLAGS) $(INCLUDES) $(LIBPATH) -o $@/space-commanderQ6 $^ $(LIBS_Q6)


#
#
#++++++++++++++++++++
# Cleanup
#--------------------
clean:
	rm -fr ./bin ./lib ./cs1_utest 




#
#++++++++++++++++++++
# buildLib 
#--------------------
staticlibs.tar: make_dir lib/libNet2Com.a
	tar -cf $@ include/space-commander/NamedPipe.h include/space-commander/Net2Com.h lib/libNet2Com.a

staticlibsQ6.tar: make_dir lib/libNet2Com-mbcc.a
	tar -cf $@ include/space-commander/NamedPipe.h include/space-commander/Net2Com.h lib/libNet2Com-mbcc.a

lib/libNamedPipe.a: $(SPACE_COMMANDER_BIN)/NamedPipe.o $(SPACE_COMMANDER_BIN)/Net2Com.o
	ar -crv $@ $^

lib/libNet2Com.a: $(SPACE_COMMANDER_BIN)/NamedPipe.o $(SPACE_COMMANDER_BIN)/Net2Com.o
	ar -crv $@ $^

lib/libNet2Com-mbcc.a: $(SPACE_COMMANDER_Q6_BIN)/NamedPipeQ6.o $(SPACE_COMMANDER_Q6_BIN)/Net2ComQ6.o
	ar -crv $@ $^




#
#
#=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
# TODO cleanup what is below
#=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#
#

buildBB:
	$(BEAGLECC) $(INCLUDES) $(DEBUGFLAGS) src/*.cpp -o bin/space-commanderBB

%.a: %.o
	ar -cvq $@ $^

bin/NamedPipeBB.o: src/NamedPipe.cpp
	$(BEAGLECC) $(MICROFLAGS) $(INCLUDES) -c $^ -o $@

bin/Net2ComBB.o: src/Net2Com.cpp
	$(BEAGLECC) $(MICROFLAGS) $(INCLUDES) -c $^ -o $@

NamedPipe-BB.a: bin/NamedPipeBB.o
	ar -cvq $@ $^

Net2Com-BB.a: src/Net2ComBB.o
	ar -cvq $@ $^
