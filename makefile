CC 	   = gcc
CFLAGS = -Wall -Wno-unused-result -Wno-pointer-sign -Wno-switch \
		 --std=gnu11 \
		 -O3 \
		  -fopenmp

IFLAGS = -Isrc -Ibuild/include
LFLAGS = -lacirc -lflint -lgmp -lm -lmmap -laesrand -lthreadpool -Lbuild/lib -Wl,-rpath -Wl,build/lib

SRCS   = $(wildcard src/*.c)
OBJS   = $(addsuffix .o, $(basename $(SRCS)))
HEADS  = $(wildcard src/*.h)

all: obfuscate evaluate

evaluate: $(OBJS) $(SRCS) $(HEADS) evaluate.c 
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) evaluate.c -o evaluate

obfuscate: $(OBJS) $(SRCS) $(HEADS) obfuscate.c 
	$(CC) $(CFLAGS) $(IFLAGS) $(LFLAGS) $(OBJS) obfuscate.c -o obfuscate

src/%.o: src/%.c 
	$(CC) $(CFLAGS) $(IFLAGS) -c -o $@ $<

deepclean: clean
	$(RM) -r libaesrand
	$(RM) -r libmmap
	$(RM) -r gghlite
	$(RM) -r clt13
	$(RM) -r libacirc
	$(RM) -r libthreadpool
	$(RM) -r build

clean:
	$(RM) src/*.o
	$(RM) *.zim
	$(RM) circuits/*.zim
	$(RM) $(OBJS)
	$(RM) obfuscate evaluate
	$(RM) vgcore.*
