all: s-talk

clean:
	-rm s-talk

s-talk: s-talk.c list.o
	gcc -o s-talk s-talk.c list.o -lpthread