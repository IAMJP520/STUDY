#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
    #include <io.h>
    #include <fcntl.h>
#endif

#define PAGESIZE (32)
#define PAS_FRAMES (256) 
#define PAS_SIZE (PAGESIZE * PAS_FRAMES)
#define VAS_PAGES (64)
#define PTE_SIZE (4)
#define PAGE_INVALID (0)
#define PAGE_VALID (1)
#define MAX_REFERENCES (256)
#define MAX_PROCESSES (10)
#define L1_PT_ENTRIES (8)
#define L2_PT_ENTRIES (8)

typedef struct {
    unsigned char frame;
    unsigned char vflag;
    unsigned char ref;
    unsigned char pad;
} pte;

typedef struct {
    int pid;
    int ref_len;
    unsigned char *references;
    pte *L1_page_table;
    int page_faults;
    int ref_count;
} process;

unsigned char pas[PAS_SIZE];
int allocated_frame_count = 0;

int allocate_frame() {
    if (allocated_frame_count >= PAS_FRAMES) 
        return -1;
    return allocated_frame_count++;
}

// 페이지 테이블 프레임을 하나 할당하고, 해당 프레임을 0으로 초기화하여 반환하는 함수
// 2단계 페이지 테이블 구조에서 1단계/2단계 모두 8개 엔트리만 필요하므로 프레임 하나만 할당
// 반환값: 할당된 페이지 테이블의 시작 주소(실패 시 NULL)
pte *allocate_pagetable_frame() {
    int frame = allocate_frame(); // 사용 가능한 프레임 번호 할당
    if (frame == -1) 
        return NULL; // 프레임 할당 실패 시 NULL 반환
    pte *page_table_ptr = (pte *)&pas[frame * PAGESIZE]; // 프레임 시작 주소를 pte 포인터로 변환
    memset(page_table_ptr, 0, PAGESIZE); // 해당 프레임(32B)을 0으로 초기화
    return page_table_ptr; // 페이지 테이블 포인터 반환
}

int load_process(FILE *fp, process *proc) {
    if (fread(&proc->pid, sizeof(int), 1, fp) != 1) 
        return 0;
    if (fread(&proc->ref_len, sizeof(int), 1, fp) != 1) 
        return 0;
    proc->references = malloc(proc->ref_len);
    if (fread(proc->references, 1, proc->ref_len, fp) != proc->ref_len) 
        return 0;

    printf("%d %d\n", proc->pid, proc->ref_len);
    for (int i = 0; i < proc->ref_len; i++) {
        printf("%02d ", proc->references[i]);
    }
    printf("\n");

    proc->page_faults = 0;
    proc->ref_count = 0;
    if ((proc->L1_page_table = allocate_pagetable_frame()) == NULL)
        return -1;
    return 1;
}

/*void simulate(process* procs, int proc_count) {

    printf("simulate() start\n");
    
    //여기에 코드 작성

    
    

    printf("simulate() end\n");
}
*/
void simulate(process* procs, int proc_count) {
    printf("simulate() start\n");

    int finished = 0;
    while (finished < proc_count) {
        finished = 0;

        for (int i = 0; i < proc_count; i++) {
            process* p = &procs[i];
            if (p->ref_count >= p->ref_len) {
                finished++;
                continue;
            }

            unsigned char vpn = p->references[p->ref_count];
            int l1_idx = vpn / L2_PT_ENTRIES;
            int l2_idx = vpn % L2_PT_ENTRIES;

            pte* l2_table;
            int l1_frame_allocated = 0;
            int l1_frame_num, l2_frame_num;

            // 1단계 PT 확인
            if (p->L1_page_table[l1_idx].vflag == PAGE_INVALID) {
                // L2 PT 프레임 새로 할당
                l2_table = allocate_pagetable_frame();
                if (!l2_table) {
                    fprintf(stderr, "L2 allocation failed (PID %d)\n", p->pid);
                    exit(1);
                }
                // L1 엔트리에 프레임 번호 저장
                l1_frame_num = ((unsigned char*)l2_table - pas) / PAGESIZE;
                p->L1_page_table[l1_idx].frame = (unsigned char)l1_frame_num;
                p->L1_page_table[l1_idx].vflag = PAGE_VALID;

                // 🔵 L1 페이지 테이블 할당도 PF로 간주
                p->page_faults++;

                l1_frame_allocated = 1;
            }
            else {
                // 이미 할당된 L2 테이블 사용
                l1_frame_num = p->L1_page_table[l1_idx].frame;
                l2_table = (pte*)&pas[l1_frame_num * PAGESIZE];
            }

            // 2단계 PT 엔트리 접근
            pte* entry = &l2_table[l2_idx];

            // 실제 페이지 프레임 할당/참조
            if (entry->vflag == PAGE_INVALID) {
                int frame = allocate_frame();
                if (frame == -1) {
                    fprintf(stderr,
                        "Frame allocation failed (pid=%d, vpn=%d)\n",
                        p->pid, vpn);
                    exit(1);
                }
                entry->frame = (unsigned char)frame;
                entry->vflag = PAGE_VALID;
                entry->ref = 1;
                p->page_faults++;
                l2_frame_num = frame;

                // 출력 (PF)
                printf("[PID %02d IDX:%03d] Page access %03d: ",
                    p->pid, p->ref_count, vpn);

                if (l1_frame_allocated) {
                    // L1도 PF
                    printf("(L1PT) PF -> Allocated Frame %03d(PTE %03d), ",
                        l1_frame_num, l1_idx);
                }
                else {
                    // L1 히트
                    printf("(L1PT) Frame %03d, ",
                        l1_frame_num);
                }

                printf("(L2PT) PF -> Allocated Frame %03d\n",
                    l2_frame_num);
            }
            else {
                // 히트
                entry->ref++;
                l2_frame_num = entry->frame;
                printf("[PID %02d IDX:%03d] Page access %03d: "
                    "(L1PT) Frame %03d, (L2PT) Frame %03d\n",
                    p->pid, p->ref_count,
                    vpn, l1_frame_num, l2_frame_num);
            }

            p->ref_count++;
        }
    }

    printf("simulate() end\n");
}



/*void print_page_tables(process* procs, int proc_count) {
    
    //여기에 코드 작성

    
    
    }
*/
void print_page_tables(process* procs, int proc_count) {
    int total_frames = 0, total_faults = 0, total_refs = 0;

    for (int i = 0; i < proc_count; i++) {
        process* p = &procs[i];
        int proc_frames = 0;

        printf("\n** Process %03d: ", p->pid);

        // Count allocated L1 + L2 entries
        for (int l1 = 0; l1 < L1_PT_ENTRIES; l1++) {
            if (p->L1_page_table[l1].vflag == PAGE_VALID) {
                proc_frames++; // L1 PT frame
                pte* l2_table = (pte*)&pas[p->L1_page_table[l1].frame * PAGESIZE];
                for (int l2 = 0; l2 < L2_PT_ENTRIES; l2++) {
                    if (l2_table[l2].vflag == PAGE_VALID) {
                        proc_frames++; // L2 PT entry = frame mapping
                    }
                }
            }
        }

        printf("Allocated Frames=%03d PageFaults/References=%03d/%03d\n", proc_frames, p->page_faults, p->ref_len);
        total_frames += proc_frames;
        total_faults += p->page_faults;
        total_refs += p->ref_len;

        for (int l1 = 0; l1 < L1_PT_ENTRIES; l1++) {
            if (p->L1_page_table[l1].vflag == PAGE_VALID) {
                int l1_frame = p->L1_page_table[l1].frame;
                printf("(L1PT) [PTE] %03d -> [FRAME] %03d\n", l1, l1_frame);

                pte* l2_table = (pte*)&pas[l1_frame * PAGESIZE];
                for (int l2 = 0; l2 < L2_PT_ENTRIES; l2++) {
                    if (l2_table[l2].vflag == PAGE_VALID) {
                        int vpn = l1 * L2_PT_ENTRIES + l2;
                        printf("(L2PT) [PAGE] %03d -> [FRAME] %03d REF=%03d\n", vpn, l2_table[l2].frame, l2_table[l2].ref);
                    }
                }
            }
        }
    }

    printf("\nTotal: Allocated Frames=%03d Page Faults/References=%03d/%03d\n", total_frames, total_faults, total_refs);
}


int main() {
    #ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
    #endif
    process procs[MAX_PROCESSES];
    int count = 0;

    printf("load_process() start\n");
    while (count < MAX_PROCESSES) {
        int ret = load_process(stdin, &procs[count]);
        if (ret == 0) 
            break;
        if (ret == -1) {
            printf("Out of memory!!\n");
            return 1;
        }
        count++;
    }
    printf("load_process() end\n");

    simulate(procs, count);
    print_page_tables(procs, count);

    for (int i = 0; i < count; i++) {
        free(procs[i].references);
    }

    return 0;
}
