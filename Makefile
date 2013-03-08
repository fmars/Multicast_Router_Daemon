pimdm:main.o callout.o debug.o igmp.o inet.o kern.o vif.o pim.o multicastpacket.o mrt.o route.o
	gcc -o pimdm main.o callout.o debug.o igmp.o inet.o kern.o vif.o pim.o multicastpacket.o mrt.o route.o
main.o: main.c defs.h
	gcc -c main.c
callout.o: callout.c defs.h
	gcc -c callout.c
multicastpacket.o:multicastpacket.c defs.h
	gcc -c multicastpacket.c
mrt.o:mrt.c defs.h
	gcc -c mrt.c
debug.o: debug.c defs.h
	gcc -c debug.c
igmp.o: igmp.c defs.h
	gcc -c igmp.c
inet.o: inet.c defs.h
	gcc -c inet.c
kern.o: kern.c defs.h
	gcc -c kern.c
vif.o: vif.c defs.h
	gcc -c vif.c
pim.o: pim.c defs.h pimdm.h
	gcc -c pim.c
route.o:route.c defs.h
	gcc -c route.c
