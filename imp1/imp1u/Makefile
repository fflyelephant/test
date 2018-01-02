APPLETS := imp1u

build := $(notdir $(APPLETS))

.PHONY: all
all: $(build)
	@echo Done.
%.o : %.c
	@echo "Building: $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	@echo "Cleaning $(APPLETS)"
	@rm -f $(APPLETS) *.o
	@if [ -f "./imp1u.h" ]; then rm -f ./imp1u.h; fi;
