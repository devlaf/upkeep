CXX                 = gcc
ARXX                = ar
AR_FLAGS            = rcs
SHARED_LIBS         = -lpthread -ldl
OUTPUT_PATH         = ./bin/
OUTPUT_FILE         = sqlite3.a
OUTPUT_INTERMEDIATE = out.o

INCLUDES = -I./

SOURCES =	./sqlite3.c

HEADERS =	./include/sqlite3.h \
			./include/sqlite3ext.h

default:
	mkdir -p $(OUTPUT_PATH)
	$(CXX) $(INCLUDES) -c -o $(OUTPUT_PATH)$(OUTPUT_INTERMEDIATE) $(SOURCES) $(SHARED_LIBS);
	$(ARXX) $(AR_FLAGS) $(OUTPUT_PATH)$(OUTPUT_FILE) $(OUTPUT_PATH)$(OUTPUT_INTERMEDIATE)
	rm -rf $(OUTPUT_PATH)$(OUTPUT_INTERMEDIATE)

clean:
	rm -rf $(OUTPUT_PATH);
