objects = adlist.o command.o dict.o rdb.o sds.o zmalloc.o\
			aefile.o db.o object.o redis.o main.o

dit : $(objects)
	clang++ -std=c++11 -o edit $(objects) 

adlist.o : adlist.h
command.o : redis.h
dict.o : dict.h
rdb.o : redis.h
sds.o : sds.h
zmalloc.o : zmalloc.h
aefile.o : redis.h
db.o : redis.h
object.o : redis.h
redis.o : redis.h
main.o : redis.h

.PYTHON : clean
clean:
	rm edit $(object)

