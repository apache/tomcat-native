include ../Makedefs

OBJS = wa_callback.o wa_connection.o wa_host.o wa_provider.o wa_provider_info.o wa_request.o

all: test libwebapp.a

clean:
	rm -f $(OBJS) libwebapp.a

libwebapp.a: $(OBJS)
	ar -cr libwebapp.a $(OBJS)

test: $(OBJS) wa.c
	$(CC) $(CFLAGS) $(INCLUDES) wa.c -o test
	rm -f test
