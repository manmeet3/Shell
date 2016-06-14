myShell:
	flex shell.l
	cc argshell.c lex.yy.c -g -o myShell -lfl

clean:
	rm -rf *.o myShell lex.yy.c *.core



