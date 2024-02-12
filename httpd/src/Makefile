include flags.mk

OBJ = main.o
BIN = httpd
LIBS = config/libconfig.a utils/libutils.a daemon/libdaemon.a logger/liblogger.a server/libserver.a http/libhttp.a

all: $(BIN)

$(BIN): $(OBJ) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) -Wl,--start-group $(LIBS) -Wl,--end-group

check:
	$(MAKE) check -C $(SRC_DIR)config
	$(MAKE) check -C $(SRC_DIR)utils
	$(MAKE) check -C $(SRC_DIR)daemon
	$(MAKE) check -C $(SRC_DIR)logger
	$(MAKE) check -C $(SRC_DIR)server
	$(MAKE) check -C $(SRC_DIR)http

clean:
	$(RM) $(OBJ) $(BIN)
	$(MAKE) clean -C $(SRC_DIR)config
	$(MAKE) clean -C $(SRC_DIR)utils
	$(MAKE) clean -C $(SRC_DIR)daemon
	$(MAKE) clean -C $(SRC_DIR)logger
	$(MAKE) clean -C $(SRC_DIR)server
	$(MAKE) clean -C $(SRC_DIR)http

include libs.mk

.PHONY: all
