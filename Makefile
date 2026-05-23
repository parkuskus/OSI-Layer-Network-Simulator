.PHONY: all run test clean

all:
	$(MAKE) -C magi_system

run:
	$(MAKE) -C magi_system run

test:
	$(MAKE) -C magi_system test

clean:
	$(MAKE) -C magi_system clean
