CC=gcc

EDIR=EXE_Files
SDIR =C_Files


_EXE = jms_console jms_coord pool
EXE = $(patsubst %,$(EDIR)/%,$(_EXE))

FIFOS = /home/lyk/Documents/FIFOS/

all: $(EXE)

jms_coord: $(EDIR)/jms_coord
	$(EDIR)/jms_coord -l $(FIFOS) -c 2	

jms_console: $(EDIR)/jms_console
	$(EDIR)/jms_console -w $(FIFOS)jmsin -r $(FIFOS)jmsout


$(EDIR)/jms_console: $(SDIR)/jms_console.c
	$(CC) -w -o $@ $^


$(EDIR)/jms_coord: $(SDIR)/jms_coord.c $(EDIR)/pool
	$(CC) -w -o $@ $(SDIR)/jms_coord.c
	

$(EDIR)/pool: $(SDIR)/pool.c
	$(CC) -w -o $@ $^


.PHONY: clean

clean:
	rm $(EDIR)/*
