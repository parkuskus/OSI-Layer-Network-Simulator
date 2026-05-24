.PHONY: all run web run-web test clean

all:
	$(MAKE) -C magi_system

run:
	$(MAKE) -C magi_system run

web:
	$(MAKE) -C magi_system web

run-web:
	$(MAKE) -C magi_system run-web PORT=$(PORT)

test:
	$(MAKE) -C magi_system test

clean:
	$(MAKE) -C magi_system clean
