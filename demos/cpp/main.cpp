#include <cstdio>
#include <cassert> //assert()
#include <cstdlib>

#define MAX_DISK_NUM (10 + 1)//存储系统中硬盘的个数最大值
#define MAX_DISK_SIZE (16384 + 1)//硬盘的存储单元个数的最大值
#define MAX_REQUEST_NUM (30000000 + 1)//某一时间片内读取对象的总次数的最大值
#define MAX_OBJECT_NUM (100000 + 1)//某一时间片内删除和写入对象的总次数都保证小于等于100000
#define REP_NUM (3)//副本数量
#define FRE_PER_SLICING (1800)
#define EXTRA_TIME (105)
//在最后的105个时间片内，输入数据保证没有删除、写入和读取请求。

typedef struct Request_ {
    int object_id;//该读取请求读取的对象的编号
    int prev_id;//该读取请求读取的对象的上一次读取请求编号
    bool is_done;//读取请求是否完成
} Request;//读取对象请求

typedef struct Object_ {
    int replica[REP_NUM + 1];//该对象的存储副本的磁盘编号数组
    int* unit[REP_NUM + 1];//该对象在存储副本磁盘上的存储单元的编号数组指针，
    //每个数组元素可能又是一个数组或链表,指向该对象在一个存储副本磁盘上的多个存储单元的编号
    int size;//对象大小
    int last_request_point;//最近一次读取请求
    bool is_delete;//是否被删除
} Object;

Request request[MAX_REQUEST_NUM];//读取请求数组
Object object[MAX_OBJECT_NUM];//对象数组

int T, M, N, V, G;
// T代表本次数据有T+105个时间片 第T+1到T+105个时间片中。没有删除、写入、读取请求
// M代表对象标签数 1<=M<=16 N代表系统中硬盘的个数 V代表硬盘中的存储单元个数
// G代表每个磁头每个时间片最多消耗的令牌数


int disk[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_point[MAX_DISK_NUM];

void timestamp_action()
{
    int timestamp;
    scanf("%*s%d", &timestamp);//%*s读取一个字符串但不保存它
    //这里是读取判题器给出的输入 TIMESTAMP current_timestamp中的TIMESTAMP
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
}

void do_object_delete(const int* object_unit, int* disk_unit, int size)
{
    //删除在该副本磁盘上，该对象的不同块
    for (int i = 1; i <= size; i++) {
        disk_unit[object_unit[i]] = 0;
        //object_unit代表在该副本磁盘上，该对象的不同块的存储单元编号数组
        //disk_unit代表编号为id的对象的第j个副本磁盘在所有磁盘数组中的编号
    }
}
//删除事件交互
void delete_action()
{
    int n_delete;//这一时间片内被删除的对象个数
    int abort_num = 0;//该时间片内被取消的读取请求的数量
    static int _id[MAX_OBJECT_NUM];//要被删除的对象的id数组

    scanf("%d", &n_delete);
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]);
    }//判题器输入

    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point;//获取该对象的最后一个请求编号
        while (current_id != 0) {
            if (request[current_id].is_done == false) {//如果该请求还没有完成
                abort_num++;//被停止的读取请求数量加一
            }
            current_id = request[current_id].prev_id;//获取该对象的上一个读取请求编号
        }
    }

    printf("%d\n", abort_num);//输出被停止的读取请求数量
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];//遍历每一个要被删除的对象，获取其编号，这里n_delete<=数组_id[]的长度
        int current_id = object[id].last_request_point;//获取该对象的最近一次读取请求编号
        while (current_id != 0) {//遍历该对象的所有读取请求（不只是在这一个时间片内）
            if (request[current_id].is_done == false) {
                printf("%d\n", current_id);//如果没有完成，就输出
            }
            current_id = request[current_id].prev_id;//获取该读取请求读取的对象的上一次读取请求编号
        }
        for (int j = 1; j <= REP_NUM; j++) {//遍历删除该对象的所有副本
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
            //object[id].unit[j] 编号为id的对象的各个块在它的第j个存储磁盘上的存储单元编号数组
            //disk[object[id].replica[j]] 编号为id的对象的第j个副本磁盘在所有磁盘数组中的编号
        }
        object[id].is_delete = true;
    }

    fflush(stdout);//强制刷新标准输出缓冲区，确保数据立即输出，不受缓冲机制影响
}

void do_object_write(int* object_unit, int* disk_unit, int size, int object_id)
{

    int current_write_point = 0;//记录已经分配了多少个存储单元
    for (int i = 1; i <= V; i++) {
        //遍历硬盘中的每一个存储单元
        if (disk_unit[i] == 0) {

            //如果当前存储单元为空，那就把这个存储单元赋值为该对象的id，表示被该对象占用
            disk_unit[i] = object_id;
            //把 这个存储单元的编号 放到 这个对象在这个存储副本硬盘中的存储单元编号数组 中
            object_unit[++current_write_point] = i;

            //这里他采用的是 顺序找空闲的存储单元 的策略

            if (current_write_point == size) {break;}
            //每一个对象快都已被分配了存储单元，退出循环
        }
    }

    assert(current_write_point == size);
    //确保对象的所有块都被分配了存储单元
    //assert(条件) 条件为true，则继续执行，否则会终止并报错
}
//写入事件交互
void write_action()
{
    int n_write;//这一时间片内要写入的对象的个数
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++) {
        //n_write可为0，为0是无需有任何输出
        //对于每一个要写入的对象
        int id, size;
        scanf("%d%d%*d", &id, &size);//获取这个对象的编号和大小
        object[id].last_request_point = 0;//置最近一次读取请求编号为0

        for (int j = 1; j <= REP_NUM; j++) {//存储到副本硬盘中
            //对象数组object[MAX_OBJECT_NUM]中第id个元素就是该对象
            // 
            //设置这个对象的存储副本的磁盘编号数组，也就是把该对象存储到那几个硬盘中
            object[id].replica[j] = (id + j) % N + 1;
            //这里它的策略是依次循环存到每一个硬盘中

            //给 这个对象在第j个存储副本硬盘中存储单元的编号数组 动态分配内存
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1)));
            //malloc(sizeof(int) * (size + 1))分配size+1个Int类型的内存空间，但返回void*
            //所以static_cast<int*>强制类型转换为Int*

            object[id].size = size;//设置对象的大小
            object[id].is_delete = false;//设置对象的状态

            do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id);
            //把 这个对象在第j个存储副本硬盘中存储单元的编号数组，
            //   这个对象的第J个存储副本硬盘的存储单元数组（它是一个数组，因为disk[]是二维数组，其第二维代表一个硬盘的存储单元）
            //   还有对象的大小、编号 作为实参传给do_object_write()
        }
        //该对象的所有存储副本都设置完毕

        printf("%d\n", id);//输出这个对象的编号
        for (int j = 1; j <= REP_NUM; j++) {
            //对于每一个副本
            printf("%d", object[id].replica[j]);//输出这个副本硬盘的编号
            for (int k = 1; k <= size; k++) {
                //输出在这个副本硬盘中，对象块的存储单元编号
                printf(" %d", object[id].unit[j][k]);
            }
            printf("\n");
        }
    }

    //输出的写入对象顺序无需和输入保持一致（但是需要在该时间片处理所有的写入请求）
    //这里他是对每一个要写入对象进行写入后随之输出的，所以是保持一致的

    fflush(stdout);
}
//读取事件交互
void read_action()
{
    int n_read;//这一时间片读取对象的个数
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++) {
        //对于每一个要读取的对象
        //获取请求编号和请求的对象编号
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;//设置该请求的请求对象编号
        request[request_id].prev_id = object[object_id].last_request_point;//记录 该读取请求读取的对象的上一次读取请求编号
        object[object_id].last_request_point = request_id;//这个对象的上一次读取请求编号置为当前读取请求的编号
        request[request_id].is_done = false;//该读取请求未完成
    }
    //读取请求数组初始化完成

    static int current_request = 0;//记录当前正在处理的请求编号
    static int current_phase = 0;//记录当前请求的阶段数
    //current_request和current_phase由static修饰，意即变量的存储位置在静态（全局）存储区，不像普通局部变量存储在栈上
    //这样的作用是read_action()返回后，变量依然存在，不会在函数调用结束时被销毁
    //变量的作用域仍然是read_action()内部，外部函数无法直接访问它
    //这两个变量是跨时间片的，而一次read_action()调用对应一个时间片，所以需要跨函数调用保存其值

    if (!current_request && n_read > 0) {
        //只有在这个时间片有读请求（n_read > 0）并且 没有之前时间片的读请求正在执行（current_request为0），才要把当前时间片的读请求设置为当前正在执行的读请求
        current_request = request_id;//初始化current_request 为当前 request_id
        //request_id 经过for循环后指向的是最后一个读取请求的编号，把它赋给current_request
        //那就是从这个时间片的最后一个读请求开始操作，为什么不从第一个开始？
    }
    if (!current_request) {
        //若 current_request 仍然为空
        for (int i = 1; i <= N; i++) {
            printf("#\n");//磁头的运动输出为"#"
        }
        printf("0\n");//这个0是n_rsp，是该时间片内上报成功的读取请求数目
    } else {
        //不为空
        current_phase++;//阶段数自增
        object_id = request[current_request].object_id;//获取当前请求的对象的id
        for (int i = 1; i <= N; i++) {
            //遍历硬盘
            if (i == object[object_id].replica[1]) {
                //如果i==这个对象的第一个存储副本硬盘的编号，也就是在第一个存储副本硬盘中
                if (current_phase % 2 == 1) {
                    //奇数阶段
                    printf("j %d\n", object[object_id].unit[1][current_phase / 2 + 1]);
                    //object[object_id].unit[1][current_phase / 2 + 1]
                    // 表示这个对象的第一个存储副本硬盘的第current_phase / 2 + 1个存储单元编号
                    //执行Jump动作，输出 j 跳跃到的存储单元编号
                    //只能在每个时间片的开始执行"Jump"动作，且执行后该磁头不能再有其他动作。
                } else {
                    //偶数阶段
                    printf("r#\n");
                    //执行read动作
                }
            } else {
                //如果不是这个对象的第一个存储副本硬盘的编号，就结束？
                // 为什么不在其他的副本硬盘中读取
                printf("#\n");
                //磁头运动结束
            }
        }

        if (current_phase == object[object_id].size * 2) {
            //如果阶段数等于读取对象的大小的两倍
            if (object[object_id].is_delete) {
                //如果这个对象被删除了
                printf("0\n");//如果0代表n_rsp当前时间片上读取完成的请求个数，那确实不用再输出本时间片上报读取完成的读取请求编号
            } else {
                //但是，如果这个对象没被删除
                printf("1\n%d\n", current_request);
                //输出1代表本时间片只有一个读取完成的请求
                //current_request是这个完成的读取请求编号
                //那不就是一个时间片上只处理一个读取请求吗
                request[current_request].is_done = true;
            }
            //时间片结束，当前的读取请求编号置为0，阶段数置为0
            current_request = 0;
            current_phase = 0;
        } else {
            //为什么不等于的时候，就是一个都没有读取完成
            printf("0\n");
        }
    }

    fflush(stdout);
}

void clean()
{
    for (auto& obj : object) {
        //对于每一个对象
        for (int i = 1; i <= REP_NUM; i++) {
            //该对象的每一个副本
            if (obj.unit[i] == nullptr)
                continue;
            //如果 该对象在其中一个副本的存储单元链表指针 不为空
            free(obj.unit[i]);
            //释放该指针指着的链表所占的内存空间
            obj.unit[i] = nullptr;
            //置该指针为空
        }
    }
}

int main()
{
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    // T代表本次数据有T+105个时间片 第T+1到T+105个时间片中。没有删除、写入、读取请求
    // M代表对象标签数 1<=M<=16
    // N代表系统中硬盘的个数
    // V代表硬盘中的存储单元个数
    // G代表每个磁头每个时间片最多消耗的令牌数

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            //处理第j个1800时间片中，要删除的标签为i的对象的大小之和（总共多少个对象快）
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            //处理第j个1800时间片中，要写入的标签为i的对象的大小之和
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            //处理第j个1800时间片中，要读取的标签为i的对象的大小之和
            scanf("%*d");
        }
    }

    printf("OK\n");
    fflush(stdout);

    //初始化硬盘数组
    for (int i = 1; i <= N; i++) {
        disk_point[i] = 1;
    }

    
    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        //处理第i个时间片的操作
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }
    clean();

    return 0;
}