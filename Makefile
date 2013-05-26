#### Compiler and tool definitions shared by all build targets #####
CC = gcc
# -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
BASICOPTS = -w -g -pthread -pipe -g3 -O6 -fPIC -DMONGO_HAVE_STDINT -DAST_MODULE=\"app_mongodb\"
CFLAGS = $(BASICOPTS)

# Define the target directories.
TARGETDIR_app_mongodb.so=build


all: $(TARGETDIR_app_mongodb.so)/app_mongodb.so

## Target: app_mongodb.so
CFLAGS_app_mongodb.so = \
	-I/usr/include/ \
	-I/usr/local/include/ \
	-I/opt/asterisk/include/ \
	-I../mongo-c-driver/src/
CPPFLAGS_app_mongodb.so = 
OBJS_app_mongodb.so =  \
	$(TARGETDIR_app_mongodb.so)/app_mongodb.o

# Link or archive
SHAREDLIB_FLAGS_app_mongodb.so = -shared -Xlinker -x -Wl,--hash-style=gnu -Wl,--as-needed -rdynamic
LDLIBS_app_mongodb.so = -lbson -lmongoc
$(TARGETDIR_app_mongodb.so)/app_mongodb.so: $(TARGETDIR_app_mongodb.so) $(OBJS_app_mongodb.so) $(DEPLIBS_app_mongodb.so)
	$(LINK.c) $(CFLAGS_app_mongodb.so) $(CPPFLAGS_app_mongodb.so) -o $@ $(OBJS_app_mongodb.so) $(SHAREDLIB_FLAGS_app_mongodb.so) $(LDLIBS_app_mongodb.so)


# Compile source files into .o files
$(TARGETDIR_app_mongodb.so)/app_mongodb.o: $(TARGETDIR_app_mongodb.so) src/app_mongodb.c
	$(COMPILE.c) $(CFLAGS_app_mongodb.so) $(CPPFLAGS_app_mongodb.so) -o $@ src/app_mongodb.c



#### Clean target deletes all generated files ####
clean:
	rm -f \
		$(TARGETDIR_app_mongodb.so)/app_mongodb.so \
		$(TARGETDIR_app_mongodb.so)/app_mongodb.o
	rm -f -r $(TARGETDIR_app_mongodb.so)


# Create the target directory (if needed)
$(TARGETDIR_app_mongodb.so):
	mkdir -p $(TARGETDIR_app_mongodb.so)
