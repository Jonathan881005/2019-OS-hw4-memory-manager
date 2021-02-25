#include <stdio.h>
#include <string.h>

#define MISS 0
#define HIT 1

#define DISK 0
#define PHY 1
#define ACTIVE 2
#define INACTIVE 3

#define NO 0
#define YES 1

#define MAX 2100000000

int find_target(int array[], int length, int lucky);
int find_victim_ESCA(int physical[], int reference[], int dirty[], int length);
int find_victim_safe(int inactive[], int inactive_history[], int reference[]);
int find_victim_SLRU(int inactive[], int inactive_history[], int reference[]);
int find_active_SLRU(int active[], int active_history[], int reference[]);
int find_free(int array[], int length);
int choose_dest(int his_phy[]);


void Set(int array[], int num, int length);
void PrintArray(int array[], int len);

int num_vpi = 0, num_pfi = 0;   //visual page / physical frame num
int inactive_length = 0, active_length = 0; //active / inactive list length
int ESCA_pointer = 0;// 用來記ESCA的指針
float line = 0; //line = 讀的行數
int history = 0;

int main()
{
    char buf[1024];
    char policy[20];
    char move[20];

    //---------------------Config------------------------------------------//
    while(fgets(buf, 100, stdin) != NULL)
    {
        if(line == 0)
            sscanf(buf, "Policy: %s", policy);
        else if(line == 1)
            sscanf(buf, "Number of Virtual Page: %d", &num_vpi);
        else if(line == 2)
            sscanf(buf, "Number of Physical Frame: %d", &num_pfi);
        if(line++ == 3)
            break;
    }
    // printf("Policy = %s, VPN = %d, PFN = %d\n", policy, num_vpi, num_pfi);

    int table_page[num_vpi];    // 目前在Physical frame or Disk block 的 index
    int inuse_page[num_vpi];    // 這張page是否出現過     1->出現過 0->沒有
    int present_page[num_vpi];  // -1->no use 0->Disk   1->Physical

    Set(table_page, -1, num_vpi);
    Set(inuse_page, 0, num_vpi);
    Set(present_page, PHY, num_vpi);


    int his_phy[num_pfi];       // Page被存入Physical的時間   FIFO

    Set(his_phy, MAX, num_pfi);

    int reference[num_vpi];     // Reference bit            ESCA
    int dirty[num_vpi];         // Dirty bit                ESCA

    Set(reference, 1, num_vpi);
    Set(dirty, 0, num_vpi);

    if(num_pfi % 2 == 0)
        inactive_length = active_length = num_pfi / 2;
    else
    {
        inactive_length = num_pfi / 2 + 1;
        active_length = num_pfi / 2;
    }

    int inactive[inactive_length];          //      Inactive list       SLRU
    int active[active_length];              //      Active   list       SLRU
    int inactive_history[inactive_length];  //      Inactive history    SLRU
    int active_history[active_length];      //      Active   history    SLRU
    int isactive[num_vpi];                  //      Active / Inactive

    Set(inactive, -1, inactive_length);
    Set(active, -1, active_length);
    Set(inactive_history, MAX, inactive_length);
    Set(active_history, MAX, active_length);
    Set(isactive, 0, num_vpi);

    int physical[num_pfi];      // Physical 中 存的是哪一張page
    int disk[num_vpi];          // Disk     中 存的是哪一張page

    Set(physical, -1, num_pfi);
    Set(disk, -1, num_vpi);


    int ishit = 0;              // Hit 與否
    int victim;                 // 哪一張page out 受害者
    int lucky;                  // 哪一張page in  幸運兒
    int dest_out_disk;          // disk swap out 的目的地
    int source_in_disk;         // disk swap in 的來源
    int target_pfi;             // 要放進去的physical frame index

    int dest_to_active,     dest_to_inactive;

    line = 0;
    float fault = 0;

    //---------------------Config------------------------------------------//

    if(strcmp(policy,"FIFO") == 0)                  //FIFO
    {

        while(fgets(buf, 100, stdin) != NULL)
        {
            ishit = 0;
            sscanf(buf, "%s %d", move, &lucky);
            if(!inuse_page[lucky])          // 沒出現過
            {
                inuse_page[lucky]   =  1;
                source_in_disk      = -1;
                target_pfi          =  find_free(physical, num_pfi);
                dest_out_disk       = -1;
                victim              = -1;

                if(target_pfi == -1)          // 沒有 free physical frame page
                {
                    target_pfi      = choose_dest(his_phy);   //找FIFO的
                    dest_out_disk   = find_free(disk, num_vpi);
                    victim          = physical[target_pfi];

                    present_page[victim] = DISK;
                    disk[dest_out_disk]  = victim;
                }

                //上面是處理output data   下面是memory/disk數據紀錄//
                physical[target_pfi] = lucky;
                his_phy[target_pfi]  = line;
            }
            else                            // 出現過
            {
                if(present_page[lucky] == DISK)     // 在disk (Miss)
                {
                    source_in_disk = find_target(disk, num_vpi, lucky);         //紀錄來源

                    target_pfi     = choose_dest(his_phy);
                    victim         = physical[target_pfi];    // 找受害者
                    dest_out_disk  = find_free(disk, num_vpi);

                    //上面是處理output data   下面是memory/disk數據紀錄//
                    disk[source_in_disk] = -1;
                    disk[dest_out_disk]  = victim;
                    physical[target_pfi] = lucky;
                    his_phy[target_pfi]  = line;

                    present_page[lucky]  = PHY;
                    present_page[victim] = DISK;
                }
                else                                // 在memory (Hit)
                {
                    target_pfi = find_target(physical, num_pfi, lucky);        //紀錄目的地 / 位置
                    ishit = 1;
                }
            }

            if(ishit)
                printf("Hit, %d=>%d\n", lucky, target_pfi);
            else
            {
                printf("Miss, %d, %d>>%d, %d<<%d\n", target_pfi, victim, dest_out_disk, lucky, source_in_disk);
                fault++;
            }

            line++;
        }
        fault = fault / line;

        printf("Page Fault Rate: %.3f\n", fault);
    }

    else if(strcmp(policy,"ESCA")==0)               //ESCA
    {

        while(fgets(buf, 100, stdin) != NULL)
        {
            ishit = MISS;
            sscanf(buf, "%s %d", move, &lucky);

            if(!inuse_page[lucky])          // 沒出現過
            {
                inuse_page[lucky] = YES;

                source_in_disk = -1;
                target_pfi     =  find_free(physical, num_pfi);
                dest_out_disk  = -1;
                victim         = -1;

                if(target_pfi == -1)          // 沒有 free physical frame page
                {
                    victim        = find_victim_ESCA(physical, reference, dirty, num_pfi);   //找ESCA的victim
                    ESCA_pointer  = ( ESCA_pointer + 1 ) % num_pfi;
                    target_pfi    = find_target(physical, num_pfi, victim);    //對應到phy裡的index
                    dest_out_disk = find_free(disk, num_vpi);

                    present_page[victim] = DISK;
                    disk[dest_out_disk]  = victim;
                }

                //上面是處理output data   下面是memory/disk數據紀錄//
                physical[target_pfi] = lucky;
                reference[lucky]     = 1;
                present_page[lucky]  = PHY;

                if(strcmp(move,"Write")==0)
                    dirty[lucky] = 1;
                else
                    dirty[lucky] = 0;

            }
            else                            // 出現過
            {
                if(present_page[lucky] == DISK)     // 在disk (Miss)
                {
                    source_in_disk = find_target(disk, num_vpi, lucky);         //紀錄來源
                    victim         = find_victim_ESCA(physical, reference, dirty, num_pfi);   //找ESCA的victim
                    ESCA_pointer   = ( ESCA_pointer + 1 ) % num_pfi;
                    target_pfi     = find_target(physical, num_pfi, victim);    //對應到phy裡的index
                    dest_out_disk  = find_free(disk, num_vpi);

                    present_page[lucky]  = PHY;
                    present_page[victim] = DISK;
                    disk[dest_out_disk]  = victim;

                    //上面是處理output data   下面是memory/disk數據紀錄//
                    physical[target_pfi] = lucky;
                    reference[lucky]     = 1;
                    disk[source_in_disk] = -1;
                    if(strcmp(move,"Write")==0)
                        dirty[lucky] = 1;
                    else
                        dirty[lucky] = 0;

                }
                else                                // 在memory (Hit)
                {
                    target_pfi = find_target(physical, num_pfi, lucky);        //紀錄目的地 / 位置
                    ishit = HIT;
                    reference[lucky] = 1;
                    if(strcmp(move,"Write")==0)
                        dirty[lucky] = 1;
                }
            }

            if(ishit)
                printf("Hit, %d=>%d\n", lucky, target_pfi);
            else
            {
                printf("Miss, %d, %d>>%d, %d<<%d\n", target_pfi, victim, dest_out_disk, lucky, source_in_disk);
                fault++;
            }

            line++;
        }
        fault = fault / line;

        printf("Page Fault Rate: %.3f\n", fault);
    }

    else if(strcmp(policy,"SLRU")==0)               //SLRU
    {

        while(fgets(buf, 100, stdin) != NULL)
        {
            ishit = MISS;
            sscanf(buf, "%s %d", move, &lucky);

            if(!inuse_page[lucky])          // 沒出現過
            {
                inuse_page[lucky] = 1;
                source_in_disk    = -1;

                target_pfi       = find_free(physical, num_pfi);
                dest_to_inactive =  find_free(inactive, inactive_length);

                if(dest_to_inactive == -1)            // 沒有 free inactive frame page
                {
                    dest_to_inactive = find_victim_SLRU(inactive, inactive_history, reference);   //找SLRU的victim
                    victim           = inactive[dest_to_inactive];
                    target_pfi       = find_target(physical, num_pfi, victim);
                    dest_out_disk    = find_free(disk, num_vpi);

                    present_page[victim] = 0;
                    disk[dest_out_disk]  = victim;
                }

                // target_pfi, victim, dest_out_disk, lucky, source_in_disk

                else                            // 有 free inactive frame page
                {
                    dest_out_disk = -1;
                    victim        = -1;
                }

                // 上面是處理output data   下面是memory/disk數據紀錄//

                physical[target_pfi]        = lucky;                   // 更新 physical
                inactive[dest_to_inactive]  = lucky;             // 填入 inactive
                inactive_history[dest_to_inactive] = history++;      // 更新 history
                isactive[dest_to_inactive]  = NO;                // 更新 isactive
                reference[lucky]            = 1;                           // 更新 reference
                present_page[lucky]         = INACTIVE;                 // 更新 present_page
            }

            else                            // 出現過
            {
                if(present_page[lucky] == DISK)     // 在disk (Miss)
                {
                    source_in_disk   = find_target(disk, num_vpi, lucky);         //紀錄來源

                    target_pfi       = find_free(physical, num_pfi);
                    dest_to_inactive =  find_free(inactive, inactive_length);
                    if(dest_to_inactive == -1)            // 沒有 free inactive frame page
                    {
                        dest_to_inactive = find_victim_SLRU(inactive, inactive_history, reference);   //找SLRU的victim
                        victim           = inactive[dest_to_inactive];    //對應到inactive裡的index
                        dest_out_disk    = find_free(disk, num_vpi);
                        target_pfi       = find_target(physical, num_pfi, victim);

                        // 上面是處理output data   下面是 memory / disk 數據紀錄 //

                        present_page[lucky]     = INACTIVE;
                        present_page[victim]    = DISK;         // 更新 present_page

                        disk[dest_out_disk]     = victim;
                        disk[source_in_disk]    = -1;

                        physical[target_pfi]       = lucky;                  // 更新 physical
                        inactive[dest_to_inactive] = lucky;           // 填入 inactive
                        inactive_history[dest_to_inactive] = history++;    // 更新 history
                        isactive[dest_to_inactive] = NO;              // 更新 isactive
                        reference[lucky]           = 1;                   // 更新 reference
                    }

                    else if(dest_to_inactive != -1)     // 有free inactive frame page
                    {
                        victim = -1;
                        dest_out_disk   = -1;
                        target_pfi      = find_target(physical, num_pfi, inactive[dest_to_inactive]);

                        // 上面是處理output data   下面是 memory / disk 數據紀錄 //

                        present_page[lucky]        = INACTIVE;     // 更新 present_page
                        disk[source_in_disk]       = -1;
                        physical[target_pfi]       = lucky;                  // 更新 physical
                        inactive[dest_to_inactive] = lucky;           // 填入 inactive
                        inactive_history[dest_to_inactive] = history++;    // 更新 history
                        isactive[dest_to_inactive] = NO;              // 更新 isactive
                        reference[lucky]           = 1;                   // 更新 reference
                    }
                }
                else                                // 在memory (Hit)
                {
                    ishit = HIT;
                    if(present_page[lucky] == ACTIVE)   // 在active list
                    {
                        dest_to_active = find_target(active, active_length, lucky);        // 紀錄 lucky 所在位置
                        target_pfi     = find_target(physical, num_pfi, lucky);
                        active_history[dest_to_active] = history++;
                        reference[lucky] = 1;
                    }

                    else if(present_page[lucky] == INACTIVE)    //在inactive list
                    {
                        if(reference[lucky] == 0)
                        {
                            dest_to_inactive= find_target(inactive, inactive_length, lucky);
                            target_pfi      = find_target(physical, num_pfi, lucky);        // 紀錄 lucky 所在位置
                            inactive_history[dest_to_inactive] = history++;
                            reference[lucky] = 1;
                        }
                        else if(reference[lucky] == 1)
                        {
                            dest_to_inactive= find_target(inactive, inactive_length, lucky);                 // 紀錄 lucky 所在位置
                            target_pfi      = find_target(physical, num_pfi, lucky);
                            dest_to_active  = find_free(active, active_length);

                            if(dest_to_active != -1)        // active list 有空位
                            {
                                physical[target_pfi]    = lucky;                  // 更新 physical
                                active[dest_to_active]  = lucky;
                                inactive[dest_to_inactive] = -1;        // 兩者swap

                                active_history[dest_to_active]      = history++;
                                inactive_history[dest_to_inactive]  = MAX; // 更新 history
                                reference[lucky]     = 0;               // 更新 reference
                                present_page[lucky]  = ACTIVE;          // 更新 presen_page
                            }

                            else                            // active list 滿了 or 不存在
                            {
                                dest_to_active = find_active_SLRU(active, active_history, reference);        // 找 active 誰該滾(的index)

                                if(dest_to_active != -1)       // active list滿了 找到victim
                                {
                                    victim = active[dest_to_active];            // 找出 victim
                                    physical[target_pfi]   = lucky;               // 更新 physical
                                    active[dest_to_active] = lucky;
                                    inactive[dest_to_inactive] = victim;        // 兩者swap

                                    active_history[dest_to_active]      = history++;
                                    inactive_history[dest_to_inactive]  = history++; // 更新 history
                                    reference[lucky]     = 0;
                                    reference[victim]    = 0;                   // 更新 reference
                                    present_page[lucky]  = ACTIVE;
                                    present_page[victim] = INACTIVE;            // 更新 present_page
                                }
                                else if(dest_to_active == -1)   // active list 長度為0 / 不存在
                                    inactive_history[dest_to_inactive] = history++;
                            }
                        }
                    }
                }
            }

            if(ishit)
                printf("Hit, %d=>%d\n", lucky, target_pfi);
            else
            {
                printf("Miss, %d, %d>>%d, %d<<%d\n", target_pfi, victim, dest_out_disk, lucky, source_in_disk);
                fault++;
            }

            line++;
        }
        fault = fault / line;
        printf("Page Fault Rate: %.3f\n", fault);
    }
    return 0;
}

int find_target( int array[], int length, int lucky)     // 在array中找存lucky的index
{
    for( int i = 0; i < length; i++)
        if(array[i] == lucky)
            return i;
    return -1;
}

int find_victim_ESCA( int physical[], int reference[], int dirty[], int length)  // 找ESCA的victim (visual page)
{
    while(1)
    {
        for(int i = 0 ; i < length; i++, ESCA_pointer = (ESCA_pointer+1)%length)     //  找00
            if(!reference[physical[ESCA_pointer]] && !dirty[physical[ESCA_pointer]])
                return physical[ESCA_pointer];

        for(int i = 0 ; i < length; i++, ESCA_pointer = (ESCA_pointer+1)%length)     //  找01
            if(!reference[physical[ESCA_pointer]] && dirty[physical[ESCA_pointer]])
                return physical[ESCA_pointer];
            else
                reference[physical[ESCA_pointer]] = 0;
    }
}

int find_victim_SLRU_safe( int inactive[], int inactive_history[], int reference[])
{
    int victim, history_min;
    while(1)
    {
        victim = -1, history_min = MAX;
        for( int i = 0 ; i < inactive_length ; i++)
            if( inactive_history[i] < history_min)
            {
                history_min = inactive_history[i];
                victim = i;
            }

        if( reference[inactive[victim]] == 1)
            reference[inactive[victim]] = 0;
        else
            return victim;
    }
}

int find_victim_SLRU( int inactive[], int inactive_history[], int reference[])
{
    int victim, history_min;
    while(1)
    {
        victim = -1, history_min = MAX;
        for( int i = 0 ; i < inactive_length ; i++)
            if( inactive_history[i] < history_min)
            {
                history_min = inactive_history[i];
                victim = i;
            }

        if( reference[inactive[victim]] == 1)
        {
            reference[inactive[victim]] = 0;
            inactive_history[victim] = history++;
        }
        else
            return victim;
    }
}

int find_active_SLRU( int active[], int active_history[], int reference[])
{
    int victim, history_min;

    if(active_length < 1)
        return -1;

    while(1)
    {
        victim = -1, history_min = MAX;
        for( int i = 0 ; i < active_length ; i++)
        {
            if( active_history[i] < history_min)
            {
                history_min = active_history[i];
                victim = i;
            }
        }

        if( reference[active[victim]] == 1)
        {
            reference[active[victim]] = 0;
            active_history[victim] = history++;
        }
        else
            return victim;
    }
}

int find_free( int array[], int length)                  // 找Free memory / disk page
{
    for( int i = 0; i < length; i++)
        if(array[i] == -1)
            return i;
    return -1;
}

int choose_dest( int his_phy[])                          // FIFO 找最先進去的
{
    int his_min = line+1;
    int dest = -1;

    for( int i = 0; i < num_pfi; i++)
        if(his_phy[i] < his_min)
        {
            his_min = his_phy[i];
            dest = i;
        }
    return dest;
}

void Set(int array[], int num, int length)              // 初始化陣列
{
    for(int i = 0; i < length; i++)
        array[i] = num;
}

void PrintArray(int array[], int len)                   // 印陣列
{
    for( int i = 0; i < len; i++)
        printf("%6d ",array[i]);
    printf("\n");
}