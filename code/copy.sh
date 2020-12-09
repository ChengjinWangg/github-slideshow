#!/bin/bash
DPATH=/home/zpp/code

IPHEAD=192.168.0.
#for i in 49 50 54 55 57 63 64 65 68
for i in 50 54 55 57 59 61 63 64 65 68 60 61  
do
echo $i
	ssh ${IPHEAD}${i} cd ${DPATH}
	if [ $? -ne 0  ]; then
	ssh ${IPHEAD}${i} mkdir -p ${DPATH}
	fi
	scp ${DPATH}/client.c root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/common.c root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/common.h root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/config root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/coordinator.c root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/makefile root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/node.c root@${IPHEAD}${i}:${DPATH}	
	scp ${DPATH}/trace root@${IPHEAD}${i}:${DPATH}	
	scp ${DPATH}/cauchy.c root@${IPHEAD}${i}:${DPATH}	
	scp ${DPATH}/cauchy.h root@${IPHEAD}${i}:${DPATH}	
	scp ${DPATH}/galois.c root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/galois.h root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/jerasure.c root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/jerasure.h root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/liberation.c root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/liberation.h root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/reed_sol.c root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/reed_sol.h root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/timing.c root@${IPHEAD}${i}:${DPATH}
	scp ${DPATH}/timing.h root@${IPHEAD}${i}:${DPATH}
done
