TOP = ../..
include $(TOP)/scripts/Makefile.common

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

libconstantbv.a: $(OBJS) depend
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@

.PHONY: clean
clean:	
	rm -rf *.o *~ *.a .#* depend

depend: $(SRCS)
	@$(CXX) -MM $(CXXFLAGS) $(SRCS) > $@

#-include depend