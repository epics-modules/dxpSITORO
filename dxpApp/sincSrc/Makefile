SRCS		= api.c command.c encode.c request.c encapsulation.c decode.c readmessage.c blocking.c listmode.c lmbuf.c socket.c discovery.c projectfile.c base64.c jsmn.c sinc.pb-c.c protobuf-c.c
OBJS		= $(SRCS:%.c=%.o)
TARGET		= libsinc-c.a
CFLAGS  	= -g -Wall -I.

all: $(TARGET)

$(TARGET): $(OBJS)
	ar r $(TARGET) $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS)

protocol:
	protoc-c --proto_path=../../proto ../../proto/sinc.proto --c_out=.

.PHONY: all clean protocol

api.o: api.c sinc.h sinc.pb-c.h sinc_internal.h
command.o: command.c sinc.h sinc.pb-c.h sinc_internal.h
encode.o: encode.c sinc.h sinc.pb-c.h sinc_internal.h
request.o: request.c sinc.h sinc.pb-c.h sinc_internal.h
encapsulation.o: encapsulation.c sinc.h sinc.pb-c.h sinc_internal.h
decode.o: decode.c sinc.h sinc.pb-c.h sinc_internal.h
readmessage.o: readmessage.c sinc.h sinc.pb-c.h sinc_internal.h
blocking.o: blocking.c sinc.h sinc.pb-c.h sinc_internal.h
listmode.o: listmode.c sinc_internal.h sinc.pb-c.h sinc.h lmbuf.h
lmbuf.o: lmbuf.c lmbuf.h
socket.o: socket.c sinc.h sinc.pb-c.h sinc_internal.h
discovery.o: discovery.c sinc.h sinc.pb-c.h sinc_internal.h discovery.h
projectfile.o: projectfile.c sinc.h sinc.pb-c.h sinc_internal.h jsmn.h
base64.o: base64.c sinc_internal.h sinc.pb-c.h sinc.h
jsmn.o: jsmn.c jsmn.h
sinc.pb-c.o: sinc.pb-c.c sinc.pb-c.h
protobuf-c.o: protobuf-c.c protobuf-c.h
