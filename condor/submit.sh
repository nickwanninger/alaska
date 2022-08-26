#!/bin/bash

GIT_ROOT=$(realpath "$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../")


# compile the benchmark
make -C ${GIT_ROOT} gap


TIMESTAMP=`date +%m-%d_%H:%M:%S` ;


function submit_job() {
	NAME=$1
	ARGS=$2

	RUN_DIR="${GIT_ROOT}/condor/runs/${TIMESTAMP}/${NAME}"
	LOG_DIR=${RUN_DIR}/log
	BIN_DIR=${RUN_DIR}/bin
	OUT_DIR=${RUN_DIR}/out
	mkdir -p ${LOG_DIR} ${BIN_DIR} ${OUT_DIR}

	CONDOR_FILE=${RUN_DIR}/job.con
	RUN_SCRIPT=${RUN_DIR}/script_for_condor_to_run.sh
	cp ${GIT_ROOT}/condor/script_for_condor_to_run.sh ${RUN_SCRIPT}

	cp ${GIT_ROOT}/condor/job.con ${CONDOR_FILE}
	EMAIL=`git config user.email`
	sed -i "s+Notify_User =+& ${EMAIL}+" ${CONDOR_FILE}
	sed -i "s+RepoPath =+& ${RUN_DIR}+" ${CONDOR_FILE}
	sed -i "s+LogDir =+& ${LOG_DIR}+" ${CONDOR_FILE}

	BIN=${BIN_DIR}/${NAME}
	cp ${GIT_ROOT}/bench/${NAME} ${BIN}

	echo "" >> ${CONDOR_FILE} ;
	echo "# Run the job" >> ${CONDOR_FILE} ;
	echo "Arguments = \"${RUN_DIR} $NAME\"" >> ${CONDOR_FILE} ;
	echo "Queue" >> ${CONDOR_FILE} ;
	echo -e "Submitting ${NAME}"

	condor_submit ${CONDOR_FILE}

}


# submit NAS
for bench in ft mg sp lu is ep cg # bt
do
	for variant in base texas
	do
		NAME="nas.${bench}.${variant}"
		submit_job $NAME # "-g 20"
	done
done

exit
for bench in gap.bfs gap.bc gap.cc gap.cc_sv gap.pr gap.pr_spmv gap.sssp # gap.tc
do
	for variant in base texas
	do
		NAME="${bench}.${variant}"
		submit_job $NAME # "-g 20"
	done
done
