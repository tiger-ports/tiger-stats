CC = cc
CFLAGS = -O2 -Wall -Wextra -Wno-deprecated-declarations -std=gnu99
APP = build/Stats-Tiger.app
BIN = $(APP)/Contents/MacOS/Stats-Tiger

.PHONY: all test clean
all: $(BIN)

$(BIN): src/main.m src/monitor.c src/monitor.h src/network.c src/network.h src/timer_support.m src/timer_support.h src/space_explorer.m src/space_explorer.h src/space_scan.c src/space_scan.h src/space_trash.c src/space_chart.c src/space_chart.h src/space_navigation.c src/space_navigation.h Info.plist
	mkdir -p $(APP)/Contents/MacOS
	$(CC) $(CFLAGS) src/main.m src/monitor.c src/network.c src/timer_support.m src/space_explorer.m src/space_scan.c src/space_trash.c src/space_chart.c src/space_navigation.c -framework Cocoa -o $(BIN)
	cp Info.plist $(APP)/Contents/Info.plist

build/test_monitor: tests/test_monitor.c src/monitor.c src/monitor.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_monitor.c src/monitor.c -o build/test_monitor

build/test_timer: tests/test_timer.m src/timer_support.m src/timer_support.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_timer.m src/timer_support.m -framework Cocoa -o build/test_timer

build/test_memory_error: tests/test_memory_error.c src/monitor.c src/monitor.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_memory_error.c src/monitor.c -o build/test_memory_error

build/test_disk: tests/test_disk.c src/monitor.c src/monitor.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_disk.c src/monitor.c -o build/test_disk

build/test_disk_reader: tests/test_disk_reader.c src/monitor.c src/monitor.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_disk_reader.c src/monitor.c -o build/test_disk_reader

build/test_network: tests/test_network.c src/network.c src/network.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_network.c src/network.c -o build/test_network

build/test_network_reader: tests/test_network_reader.c src/network.c src/network.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_network_reader.c -o build/test_network_reader

build/test_space_scan: tests/test_space_scan.c src/space_scan.c src/space_scan.h
	mkdir -p build
	$(CC) $(CFLAGS) -DST_SPACE_TESTING tests/test_space_scan.c src/space_scan.c -o $@

build/test_space_chart: tests/test_space_chart.c src/space_chart.c src/space_chart.h src/space_scan.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_space_chart.c src/space_chart.c -o $@

build/test_space_navigation: tests/test_space_navigation.c src/space_navigation.c src/space_navigation.h src/space_scan.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_space_navigation.c src/space_navigation.c -o $@

build/test_space_thread: tests/test_space_thread.m src/space_explorer.m src/space_explorer.h src/space_scan.c src/space_scan.h src/space_trash.c src/space_chart.c src/space_chart.h src/space_navigation.c src/space_navigation.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_space_thread.m src/space_explorer.m src/space_scan.c src/space_trash.c src/space_chart.c src/space_navigation.c -framework Cocoa -o $@

build/test_space_refresh: tests/test_space_refresh.m src/space_explorer.m src/space_explorer.h src/space_scan.c src/space_scan.h src/space_trash.c src/space_chart.c src/space_chart.h src/space_navigation.c src/space_navigation.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_space_refresh.m src/space_explorer.m src/space_scan.c src/space_trash.c src/space_chart.c src/space_navigation.c -framework Cocoa -o $@

build/test_space_refresh_live: tests/test_space_refresh_live.m src/space_explorer.m src/space_explorer.h src/space_scan.c src/space_scan.h src/space_trash.c src/space_chart.c src/space_chart.h src/space_navigation.c src/space_navigation.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_space_refresh_live.m src/space_explorer.m src/space_scan.c src/space_trash.c src/space_chart.c src/space_navigation.c -framework Cocoa -o $@

build/test_space_trash: tests/test_space_trash.c src/space_scan.c src/space_scan.h src/space_trash.c
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_space_trash.c src/space_scan.c src/space_trash.c -o $@

build/test_space_trash_ui: tests/test_space_trash_ui.m src/space_explorer.m src/space_explorer.h src/space_scan.c src/space_scan.h src/space_trash.c src/space_chart.c src/space_chart.h src/space_navigation.c src/space_navigation.h
	mkdir -p build
	$(CC) $(CFLAGS) tests/test_space_trash_ui.m src/space_explorer.m src/space_scan.c src/space_trash.c src/space_chart.c src/space_navigation.c -framework Cocoa -o $@

test: build/test_monitor build/test_timer build/test_memory_error build/test_disk build/test_disk_reader build/test_network build/test_network_reader build/test_space_scan build/test_space_chart build/test_space_navigation build/test_space_thread build/test_space_refresh build/test_space_refresh_live build/test_space_trash build/test_space_trash_ui $(BIN)
	./build/test_network
	./build/test_network_reader
	./build/test_monitor
	./build/test_timer
	./build/test_memory_error
	./build/test_disk
	./build/test_disk_reader
	mkdir -p build/test-tmp
	TMPDIR="$(CURDIR)/build/test-tmp" ./build/test_space_scan
	./build/test_space_chart
	./build/test_space_navigation
	TMPDIR="$(CURDIR)/build/test-tmp" ./build/test_space_thread
	./build/test_space_refresh
	TMPDIR="$(CURDIR)/build/test-tmp" ./build/test_space_refresh_live
	TMPDIR="$(CURDIR)/build/test-tmp" ./build/test_space_trash
	TMPDIR="$(CURDIR)/build/test-tmp" ./build/test_space_trash_ui
	sh tests/test_cli.sh $(BIN)

clean:
	rm -rf build
