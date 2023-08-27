CONTIKI_PROJECT = hono-mqtt-client
all: $(CONTIKI_PROJECT)

CONTIKI = ../..

MAKE_MAC = MAKE_MAC_TSCH
#MODULES += os/services/orchestra
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/mqtt
#MODULES += os/services/rpl-border-router

include $(CONTIKI)/Makefile.identify-target

MODULES_REL += arch/platform/$(TARGET)


PLATFORMS_ONLY = cc26x0-cc13x0 cc2538dk openmote zoul native simplelink cooja

include $(CONTIKI)/Makefile.include
