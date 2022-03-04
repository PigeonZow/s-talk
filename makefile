all: s-talk

clean:
	-rm s-talk

s-talk: s-talk.c list.o
	gcc -Wall -o s-talk s-talk.c list.o -lpthread