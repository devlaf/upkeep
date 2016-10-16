PROJECT = upkeep

LINUX_CXX               = g++
LINUX_CPPFLAGS          = -Wall -D_GNU_SOURCE $(LIB_PATH) -Wno-write-strings -DLINUX
LINUX_DEBUGFLGS	        = -g
LINUX_SHARED_LIBS       = -lpthread -ldl -lzlog
LINUX_LINKER_PATH       = -L./libs/zlog/build/lib/
LINUX_LD_FLAGS          = -Wl,-rpath,'$$ORIGIN'
LINUX_STAT_LIBS_DEGUG   = ./libs/libuv/out/Debug/libuv.a ./libs/sqlite/bin/sqlite3.a
LINUX_STAT_LIBS_RELEASE = ./libs/libuv/out/Release/libuv.a ./libs/sqlite/bin/sqlite3.a
DEBUG_OUTPUT_PATH       = "./bin/DEBUG/"
RELEASE_OUTPUT_PATH     = "/opt/upkeep/bin/"

INCLUDES = -I./include/\
           -I./libs/libuv/include/\
           -I./libs/zlog/build/include/\
           -I./libs/sqlite/

SOURCES =	src/main.cpp \
            src/logger.cpp \
            src/database.cpp

HEADERS =	./include/logger.h \
			./include/database.h \
			./libs/sqlite/sqlite3.h \
			./libs/sqlite/sqlite3ext.h

default:
	$(info ******** No target build specified.  Available targets are: linux, debuglinux, clean. ********)

linux:
	sudo mkdir -p $(RELEASE_OUTPUT_PATH)
	cd libs/sqlite/ && $(MAKE)
	cd libs/zlog/ && $(MAKE) PREFIX=../build
	cd libs/zlog/ && sudo $(MAKE) PREFIX=../build install
	sudo cp libs/zlog/build/lib/libzlog.so* $(RELEASE_OUTPUT_PATH)
	sudo $(LINUX_CXX) $(INCLUDES) $(LINUX_CPPFLAGS) -o $(RELEASE_OUTPUT_PATH)$(PROJECT) $(SOURCES) $(LINUX_STAT_LIBS_RELEASE) $(LINUX_LD_FLAGS) $(LINUX_LINKER_PATH) $(LINUX_SHARED_LIBS);

debuglinux:
	mkdir -p $(DEBUG_OUTPUT_PATH)
	cd libs/sqlite/ && $(MAKE)
	cd libs/zlog/ && $(MAKE) PREFIX=../build
	cd libs/zlog/ && sudo $(MAKE) PREFIX=../build install
	cp libs/zlog/build/lib/libzlog.so* $(DEBUG_OUTPUT_PATH)
	$(LINUX_CXX) $(INCLUDES) $(LINUX_CPPFLAGS) $(LINUX_DEBUGFLGS) -o $(DEBUG_OUTPUT_PATH)$(PROJECT) $(SOURCES) $(LINUX_STAT_LIBS_DEGUG) $(LINUX_LD_FLAGS) $(LINUX_LINKER_PATH) $(LINUX_SHARED_LIBS);

osx:
	$(info ******** Target build not supported at this time. It's on the (growing) TODO list! ********)

debugosx:
	$(info ******** Target build not supported at this time. It's on the (growing) TODO list! ********)

clean:
	cd libs/sqlite/ && $(MAKE) clean
	rm -rf $(DEBUG_OUTPUT_PATH) && sudo rm -rf $(RELEASE_OUTPUT_PATH);
