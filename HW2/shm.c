#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/time.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<unistd.h>

unsigned int* createSHMBlock(int dim, int* shmid);
void singleProcess(unsigned int* A, unsigned int* B, unsigned int* C, int dim);
void fourProcess(unsigned int* A, unsigned int* B, unsigned int* C, int dim);

int main(int argc, char* argv[]) {
    int dim;
    scanf("%d", &dim);

    int shm_id[3] = {0,0,0};

    unsigned int *arrA = createSHMBlock(dim, &shm_id[0]);
    unsigned int *arrB = createSHMBlock(dim, &shm_id[1]);
    unsigned int *arrC = createSHMBlock(dim, &shm_id[2]);

    // initial the same matitx value in arrA and arrB, arrA and arrB are consecutive memory block
    for(int i=0; i<dim; i++) {
        for(int j=0; j<dim; j++) {
            arrA[i*dim+j] = i*dim+j;
            arrB[i*dim+j] = i*dim+j;
        }
    }

    singleProcess(arrA, arrB, arrC, dim);
    fourProcess(arrA, arrB, arrC, dim);

    // since we attach the shared memory to current process, we need to detach from current process (something like put it back to the background) once we done the tasks
    shmdt(arrA);
    shmdt(arrB);
    shmdt(arrC);

    // delete shared memory (release)
     for (int i=0; i<3; i++)
        shmctl(shm_id[i], IPC_RMID, NULL);

    return 0;
}

unsigned int* createSHMBlock(int dim, int* shmid) {
    *shmid = shmget(0, sizeof(unsigned int*)*dim*dim, IPC_CREAT | 0666);
    if(*shmid<0) {
        perror("create share memory block error");
        exit(EXIT_FAILURE);
    }
    unsigned int *shmaddr = shmat(*shmid, NULL, 0);
    if(shmaddr < 0) {
        perror("attach shared memory to current process error");
        exit(EXIT_FAILURE);
    }
    return shmaddr;
}

void singleProcess(unsigned int* A, unsigned int* B, unsigned int* C, int dim) {
    struct timeval start, end;
    // initial C value as 0
    for(int i=0; i<dim; i++) {
        for(int j=0; j<dim; j++) {
            C[i*dim+j] = 0;
        }
    }

    gettimeofday(&start, 0);
    unsigned int csum = 0;

    for (int i=0; i<dim; ++i){
        for (int j=0; j<dim; ++j){
            for (int k=0; k<dim; ++k){
                C[i*dim+j] += (A[i*dim+k] * B[k*dim+j]);
            }
            csum += C[i*dim+j];
        }
    }

    gettimeofday(&end, 0);

    int sec = end.tv_sec - start.tv_sec;
    int usec = end.tv_usec - start.tv_usec;

    printf("1-process, checksum = %u\n", csum);
    printf("elapsed %f ms\n", sec*1000+(usec/1000.0));
}

void fourProcess(unsigned int* A, unsigned int* B, unsigned int* C, int dim) {
    struct timeval start, end;
    int p1,p2,p3,p4;
    p1 = p2 = p3 = p4 = dim/4;
    int tmp = dim;
    dim%=4;
    // amortize amount of rows for each process
    if(dim > 0) {
        p1++;
        dim--;
    }
    if(dim > 0) {
        p2++;
        dim--;
    }
    if(dim > 0) {
        p3++;
        dim--;
    }
    dim = tmp;
    // initial C as 0
    for(int i=0; i<dim; i++) {
        for(int j=0; j<dim; j++) {
            C[i*dim+j] = 0;
        }
    }

    pid_t pid1,pid2,pid3;
    int status;

    gettimeofday(&start, 0);

    pid1 = fork();
    if(pid1 == 0) {
        for(int i = 0; i < p1; i++) {
            for(int j=0; j<dim; j++) {
                for(int k=0; k<dim; k++) {
                    C[i*dim+j] += (A[i*dim+k]*B[k*dim+j]);
                }
            }
        }
        exit(EXIT_SUCCESS);
    } else if(pid1 < 0) {
        perror("process 1 error");
        exit(EXIT_FAILURE);
    } else {
        pid2 = fork();
        if(pid2 == 0) {
            for(int i=p1; i<p1+p2; i++) {
                for(int j=0; j<dim; j++) {
                    for(int k=0; k<dim; k++) {
                        C[i*dim+j] += (A[i*dim+k]*B[k*dim+j]);
                    }
                }
            }
            exit(EXIT_SUCCESS);
        } else if(pid2 < 0) {
            perror("process 2 error");
            exit(EXIT_FAILURE);
        } else {
            pid3 = fork();
            if(pid3 == 0) {
                for(int i=p1+p2; i<p1+p2+p3; i++) {
                    for(int j=0; j<dim; j++) {
                        for(int k=0; k<dim; k++) {
                            C[i*dim+j] += (A[i*dim+k]*B[k*dim+j]);
                        }
                    }
                }
                exit(EXIT_SUCCESS);
            } else if(pid3 < 0) {
                perror("process 3 error");
                exit(EXIT_FAILURE);
            } else {
                for(int i=p1+p2+p3; i<dim; i++) {
                    for(int j=0;j<dim; j++) {
                        for(int k=0; k<dim; k++) {
                            C[i*dim+j] += (A[i*dim+k]*B[k*dim+j]);
                        }
                    }
                }

                // main process
                // wait process 3 complete
                waitpid(pid3, NULL, 0);
            }

            // main process
            // wait process 2 complete
            waitpid(pid2, NULL, 0);
        }

        // main process
        // wait process 1 complete
        waitpid(pid1, NULL, 0);
    }

    int csum = 0;
    for(int i=0; i<dim; i++) {
        for(int j=0; j<dim; j++) {
            csum+=C[i*dim+j];
        }
    }

    gettimeofday(&end, 0);

    int sec = end.tv_sec - start.tv_sec;
    int usec = end.tv_usec - start.tv_usec;

    printf("4-process, checksum = %u\n", csum);
    printf("elapsed %f ms\n", sec*1000+(usec/1000.0));
}

/*
                                             block        block
    main process shared memory block  <-->   Child1 <---> Child2 <---> Child3 
                                      
*/