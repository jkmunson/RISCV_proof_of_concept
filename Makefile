MAKEFLAGS+="j=32"

CC:=g++
CCWIN64:=x86_64-w64-mingw32-g++

compiler_flags:=-march=native -Wpedantic -Wall -Wextra -Os -I./src/ -fdata-sections -ffunction-sections -flto -g -fno-strict-aliasing
linker_flags:= -Wl,--gc-sections -lSDL2main -lSDL2 -lSDL2_image -pthread -lpthread -flto

windows_extra_compiler_flags:=-mconsole -mwindows -I./externalLibs/include
windows_dynamic_linker_flags:=-lmingw32 -I./externalLibs/include -L./externalLibs/lib/SDL2
windows_static_linker_flags=$(windows_dynamic_linker_flags) -static $(linker_flags) -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid

linux_executable:=./bin/RISCYLinux
linux_debug_executable:=./bin/RISCYLinuxDebug
windows_executable:=./bin/RISCYWin64.exe
windows_static_executable:=./bin/RISCYWin64_static.exe

headers:=$(wildcard ./src/*.h) Makefile
objectsL64:=$(patsubst ./src/%,./objL64/%, $(patsubst %.cpp,%.o,$(wildcard ./src/*.cpp)))
objectsW64:=$(patsubst ./src/%,./objW64/%, $(patsubst %.cpp,%.o,$(wildcard ./src/*.cpp)))
asm_files:=$(patsubst ./src/%,./asm/%, $(patsubst %.cpp,%.s,$(wildcard ./src/*.cpp)))

linux: $(linux_executable)
$(linux_executable): $(objectsL64)
	$(CC) -o $@ $^ $(compiler_flags) $(linker_flags) -s
	strip $(linux_executable) -s -R .comment -R .gnu.version
	
debug: linux-debug
linux-debug: $(linux_debug_executable)
$(linux_debug_executable): $(objectsL64)
	$(CC) -o $@ $^ $(compiler_flags) $(linker_flags)

./objL64/%.o: ./src/%.cpp $(headers)
	$(CC) -c -o $@ $< $(compiler_flags)

.PHONY: all
all: $(linux_executable) $(windows_executable) $(windows_static_executable) $(linux_debug_executable)

windows: $(windows_executable)
$(windows_executable): $(objectsW64)
	$(CCWIN64) -o $@ $^ $(compiler_flags) $(windows_dynamic_linker_flags) $(linker_flags) $(windows_extra_compiler_flags) -s
	
windows-static: $(windows_static_executable)
$(windows_static_executable):$(objectsW64)
	$(CCWIN64) -o $@ $^ $(compiler_flags) $(windows_static_linker_flags) $(windows_extra_compiler_flags) -s

./objW64/%.o: ./src/%.c $(headers)
	$(CCWIN64) -c -o $@ $< $(compiler_flags) $(windows_extra_compiler_flags)

.PHONY: clean
clean:
	-rm ./bin/* -r
	-rm ./objW64/* -r
	-rm ./objL64/* -r
	-rm ./asm/* -r

.PHONY: valgrind
valgrind: $(linux_debug_executable)
	valgrind --leak-check=full --num-callers=50 --suppressions=valgrind-suppressions $(linux_debug_executable) > /dev/null

.PHONY: assembly
assembly: $(asm_files)
./asm/%.s: ./src/%.c $(headers)
	$(CC) -S -o $@ $< $(compiler_flags) -fno-lto -s
