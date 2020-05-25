CC      = gcc  
CFLAGS  = -Wall  
LDFLAGS = -lpthread
  
TARGET = epoll_thread 
OBJECTS = main.o thread_pool.o conn_event.o 
  
# make target  
all:${OBJECTS}  
	${CC} -o ${TARGET} *.o ${LDFLAGS} 
	      
# make clean  
clean:  
	rm -rf ${TARGET} ${OBJECTS}   
	  
# dependence      
%.o : %.c 
	${CC} ${CFLAGS} -c $< -o $@  
