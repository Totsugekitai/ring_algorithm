.RECIPEPREFIX = >

CFLAGS := -Wall -g -O `pkg-config --cflags margo` -lpthread
LDLIBS := `pkg-config --libs margo`

OBJ := main.o ring.o rpc.o
TARGET := prog

.PHONY: default
> make $(TARGET)

.PHONY: $(TARGET)
$(TARGET): $(OBJ)
> cc $(CFLAGS) $(LDLIBS) -o $@ $+

.PHONY: clean
clean:
> rm -f $(TARGET) *.o

