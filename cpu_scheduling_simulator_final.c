#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_PROCESSES 8
#define MAX_TIME 75
#define TIME_QUANTUM 2
#define MAX_ALGORITHM_NUM 10
#define MAX_IO_REQUESTS 3

typedef enum {
    FCFS_ALG,
    SJF_ALG,
    PSJF_ALG,
    PRIORITY_ALG,
    PPRIORITY_ALG,
    RR_ALG
} SchedulingAlgorithm;

typedef struct {
    int pid;
    int arrival_time;
    int cpu_burst;
    int io_burst[MAX_IO_REQUESTS];
    int io_request_time[MAX_IO_REQUESTS];
    int io_count;
    int current_io_index;
    int priority;
    int executed_time;
    int io_remaining_time;
    int finished;
    int waitingTime;
    int turnaroundTime;
    int responseTime;
} process;

process original[MAX_PROCESSES]; // 처음 프로세스들 저장 큐
process working[MAX_PROCESSES]; // 클론 해서 쓸 것들
int process_count = 0;

process* ready_queue[MAX_PROCESSES]; //대기 큐
int ready_count = 0;

process* waiting_queue[MAX_PROCESSES]; //i/o 버스트 하는 큐
int waiting_count = 0;

process* terminated[MAX_PROCESSES]; // 종료된 프로세스들 큐
int terminated_count = 0;

process* running_process = NULL;

int gantt_chart[MAX_TIME];
int timeConsumed = 0;

//=====================================

void add_ready_queue(process* p) {
    if (ready_count < MAX_PROCESSES)
        ready_queue[ready_count++] = p;
}

void remove_ready_queue(int index) {
    for (int i = index; i < ready_count - 1; i++)
        ready_queue[i] = ready_queue[i + 1];
    ready_queue[--ready_count] = NULL;
}

void add_to_waiting_queue(process* p) {
    if (waiting_count < MAX_PROCESSES)
        waiting_queue[waiting_count++] = p;
}

void remove_waiting_queue(int index) {
    for (int i = index; i < waiting_count - 1; i++)
        waiting_queue[i] = waiting_queue[i + 1];
    waiting_queue[--waiting_count] = NULL;
}

//================================================
void generate_processes(int count) {
    for (int i = 0; i < count; i++) {
        original[i].pid = i + 1;
        original[i].arrival_time = rand() % 20;
        original[i].cpu_burst = 3 + rand() % 12;
        original[i].io_count = rand() % (MAX_IO_REQUESTS+1); // 0~3회
        int used_slots[15] = {0};  
        for (int j = 0; j < original[i].io_count; j++) {
            int req_time;
            do {
                req_time = 1 + rand() % (original[i].cpu_burst - 1);
            } while (used_slots[req_time]);  
            used_slots[req_time] = 1;
            original[i].io_request_time[j] = req_time;
            original[i].io_burst[j] = 1 + rand() % 6;  
        }
        original[i].current_io_index = 0;
        original[i].priority = 1 + rand() % 10;
        original[i].executed_time = 0;
        original[i].io_remaining_time = 0;
        original[i].finished = 0;
        original[i].waitingTime = 0;
        original[i].turnaroundTime = 0;
        original[i].responseTime = -1;

        for (int j = 0; j < original[i].io_count - 1; j++) {
            for (int k = j + 1; k < original[i].io_count; k++) {
                if (original[i].io_request_time[j] > original[i].io_request_time[k]) {

                    int tmp_time = original[i].io_request_time[j];
                    original[i].io_request_time[j] = original[i].io_request_time[k];
                    original[i].io_request_time[k] = tmp_time;

                    int tmp_burst = original[i].io_burst[j];
                    original[i].io_burst[j] = original[i].io_burst[k];
                    original[i].io_burst[k] = tmp_burst;
                }
            }
        }
    }
}

void clone_processes() {
    for (int i = 0; i < process_count; i++)
        working[i] = original[i];
}
// ================================

typedef struct evaluation* evalPointer;
typedef struct evaluation {
	int alg;
	int preemptive;
	int startTime;
	int endTime;
	int avg_waitingTime;
	int avg_turnaroundTime;
	int avg_responseTime;
	double CPU_util;
	int completed;
}evaluation;

evalPointer evals[MAX_ALGORITHM_NUM];
int cur_eval_num = 0;
//초기화
void init_evals(){
	cur_eval_num = 0;
	int i;
	for(i=0;i<MAX_ALGORITHM_NUM;i++)
		evals[i]=NULL;
}
// 삭제
void clear_evals() {
	
	int i;
	for(i=0;i<MAX_ALGORITHM_NUM;i++){
		free(evals[i]);
		evals[i]=NULL;
	}
	cur_eval_num = 0;
}

void evaluate_simulation(SchedulingAlgorithm alg, const char* name, int total_time) {
    int start_time = -1, end_time = -1;
    int total_waiting = 0, total_turnaround = 0, total_response = 0;
    int cpu_active_time = 0;

    for (int i = 0; i < total_time; i++) {
        if (gantt_chart[i] != 0) {
            if (start_time == -1) start_time = i;
            end_time = i;
            cpu_active_time++;
        }
    }

    for (int i = 0; i < process_count; i++) {
        total_waiting += working[i].waitingTime;
        total_turnaround += working[i].turnaroundTime;
        total_response += working[i].responseTime;
    }

    evaluation* e = (evaluation*)malloc(sizeof(evaluation));
    e->alg = alg;
    e->preemptive = (alg == PSJF_ALG || alg == PPRIORITY_ALG || alg == RR_ALG);
    e->startTime = (start_time == -1) ? 0 : start_time;
    e->endTime = (end_time == -1) ? 0 : end_time + 1;  // end time is exclusive
    e->avg_waitingTime = total_waiting / process_count;
    e->avg_turnaroundTime = total_turnaround / process_count;
    e->avg_responseTime = total_response / process_count;
    e->CPU_util = ((double)cpu_active_time) / (e->endTime - e->startTime) * 100.0;
    e->completed = terminated_count;

    evals[cur_eval_num++] = e;

    printf("=== %s Evaluation ===\n", name);
    printf("Start Time       : %d\n", e->startTime);
    printf("End Time         : %d\n", e->endTime);
    printf("Avg Waiting Time : %d\n", e->avg_waitingTime);
    printf("Avg Turnaround   : %d\n", e->avg_turnaroundTime);
    printf("Avg Response     : %d\n", e->avg_responseTime);
    printf("CPU Utilization  : %.2f%%\n", e->CPU_util);
    printf("Completed        : %d\n\n", e->completed);
}


// ====================
process* select_process_fcfs() {
    if (ready_count == 0) return NULL;
    int index = 0;

    process* selected = ready_queue[index];

    remove_ready_queue(index);
    return selected;
}

process* select_process_sjf() {
    if (ready_count == 0) return NULL;
    process* selected = ready_queue[0];
    for (int i = 1; i < ready_count; i++) {
        if (ready_queue[i]->cpu_burst < selected->cpu_burst)
            selected = ready_queue[i];
    }
    int index = -1;
    for (int i = 0; i < ready_count; i++) {
        if (ready_queue[i] == selected) { index = i; break; }
    }
    remove_ready_queue(index);
    return selected;
}

process* select_process_priority() {
    if (ready_count == 0) return NULL;
    process* selected = ready_queue[0];
    for (int i = 1; i < ready_count; i++) {
        if (ready_queue[i]->priority < selected->priority)
            selected = ready_queue[i];
    }
    int index = -1;
    for (int i = 0; i < ready_count; i++) {
        if (ready_queue[i] == selected) { index = i; break; }
    }
    remove_ready_queue(index);
    return selected;
}

process* select_process_RR(int time_quantum){
    process* selected = ready_queue[0];
    if (selected != NULL){
        int index = -1;
        for (int i = 0; i < ready_count; i++) {
            if (ready_queue[i] == selected) { index = i; break; }
        }
        if(running_process != NULL) { //이미 수행중인 프로세스가 있었다면
				//return runningProcess;
				
			if(timeConsumed >= time_quantum){ //이미 수행중이 었던 프로세스가 Time expired되었다면 
                remove_ready_queue(index);		
				add_ready_queue(running_process);
                return selected;
			} 
            else {
					return running_process;
			}
				
        } 
        else {
            remove_ready_queue(index);
            return selected;
		}
        
    } 
    else { //readyQueue에 아무것도 없는 경우
            return running_process; 
    }

}

process* schedule(SchedulingAlgorithm alg) {
    if (alg == FCFS_ALG) return (running_process != NULL) ? running_process : select_process_fcfs();
    if (alg == SJF_ALG)  return (running_process != NULL) ? running_process : select_process_sjf();
    if (alg == PSJF_ALG) {
        process* candidate = select_process_sjf();
        if (candidate == NULL) return running_process;
        if (running_process == NULL) return candidate;
        if (candidate != NULL && candidate->cpu_burst < running_process->cpu_burst) {
            add_ready_queue(running_process);
            return candidate;
        }
        add_ready_queue(candidate);
        return running_process;
    }
    if (alg == PRIORITY_ALG)
        return (running_process != NULL) ? running_process : select_process_priority();
    if (alg == PPRIORITY_ALG) {
        process* candidate = select_process_priority();
        if (candidate == NULL) return running_process;
        if (running_process == NULL) return candidate;
        if (candidate != NULL && candidate->priority < running_process->priority) {
            add_ready_queue(running_process);
            return candidate;
        }
        add_ready_queue(candidate);
        return running_process;
    }
    if (alg == RR_ALG) {
        return select_process_RR(TIME_QUANTUM);
    }
    return NULL;
}

// ===================

void init_simulation() {
    ready_count = waiting_count = terminated_count = 0;
    running_process = NULL;
    timeConsumed = 0;
    for (int i = 0; i < MAX_TIME; i++) gantt_chart[i] = 0;
}

void tick(int current_time, SchedulingAlgorithm alg) {
    for (int i = 0; i < process_count; i++) {
        if (working[i].arrival_time == current_time && working[i].finished == 0)
            add_ready_queue(&working[i]);
    } // arrive time에 맞추어 집어넣기

    process* prev_process = running_process;
    running_process = schedule(alg);

    if(running_process != prev_process){
        timeConsumed = 0;
        if(running_process->responseTime == -1){
            running_process->responseTime = current_time - running_process->arrival_time;
        }
    }

    for (int i = 0; i< process_count;i++){
        if(ready_queue[i]){
            ready_queue[i]->waitingTime++;
            ready_queue[i]->turnaroundTime++;
        }
    }

    for (int i = 0; i < waiting_count; i++) {
        process* p = waiting_queue[i];
        p->turnaroundTime++;
        p->io_remaining_time--;
        if (p->io_remaining_time == 0) {
            add_ready_queue(p);
            remove_waiting_queue(i);
            i--;
        }
    } //waiting 큐에서 io 시간 처리& 복귀

    if (running_process != NULL) {
        running_process->executed_time++;
        running_process->cpu_burst--;
        timeConsumed++;
        gantt_chart[current_time] = running_process->pid;

        if (running_process->current_io_index < running_process->io_count &&
        running_process->executed_time == running_process->io_request_time[running_process->current_io_index]) {

            running_process->io_remaining_time = running_process->io_burst[running_process->current_io_index];
            running_process->current_io_index++;

            add_to_waiting_queue(running_process);
            running_process = NULL;
            return;
        }

        if (running_process->cpu_burst == 0) {
            running_process->finished = 1;
            terminated[terminated_count++] = running_process;
            running_process = NULL;
        }
    } else {
        gantt_chart[current_time] = 0;
    }
}

void print_gantt_chart(const char* name, int total_time) {
    printf("=== %s ===\n", name);
    printf("Gantt Chart:\n");
    for (int i = 0; i < total_time; i++) {
        if (gantt_chart[i] == 0) printf("| idle ");
        else printf("| P%d ", gantt_chart[i]);
    }
    printf("|\n\n");
}

void run_simulation(SchedulingAlgorithm alg, const char* name) {
    clone_processes();
    init_simulation();
    for (int time = 0; time < MAX_TIME; time++) {
        tick(time, alg);
        if (terminated_count == process_count) break;
    }
    print_gantt_chart(name, MAX_TIME);
    evaluate_simulation(alg, name, MAX_TIME);
}

int main() {
    srand(time(NULL));
    process_count = 3 + rand() % (MAX_PROCESSES - 2);
    generate_processes(process_count);
    init_evals();
    printf("PID\tArrival\tCPU\tI/O Requests\t\tPriority\n");
    for (int i = 0; i < process_count; i++) {
        printf("%d\t%d\t%d\t", 
            original[i].pid, original[i].arrival_time, original[i].cpu_burst);

        if (original[i].io_count == 0) {
            printf("None\t\t\t");
        } else {
            for (int j = 0; j < original[i].io_count; j++) {
                printf("(%d@%d) ", 
                    original[i].io_burst[j], original[i].io_request_time[j]);
            }
            if (original[i].io_count == 1) printf("\t\t\t");
            else if (original[i].io_count == 2) printf("\t\t");
            else if (original[i].io_count == 3) printf("\t");
        }

        printf("%d\n", original[i].priority);
    }


    run_simulation(FCFS_ALG, "First Come First Served");
    run_simulation(SJF_ALG,  "Shortest Job First");
    run_simulation(PSJF_ALG, "Preemptive SJF");
    run_simulation(PRIORITY_ALG, "Priority Scheduling (Non-preemptive)");
    run_simulation(PPRIORITY_ALG, "Preemptive Priority Scheduling");
    run_simulation(RR_ALG, "Round Robin (Quantum = 2)");
    clear_evals();
    return 0;
}
