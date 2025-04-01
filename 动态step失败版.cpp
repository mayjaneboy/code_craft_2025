#define _CRT_SECURE_NO_WARNINGS

#pragma GCC optimize(2)

#include <cstdio>
#include <cassert> //assert()
#include <cstdlib>
#include <algorithm>
#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <cmath>
#include <numeric>
#include <random>
#include <unordered_map>
#include <cstring> // 添加此行以解决memset未定义的问题
#include <unordered_set>

using namespace std;

#define MAX_DISK_NUM (10 + 1)          // 存储系统中硬盘的个数最大值
#define MAX_DISK_SIZE (16384 + 1)      // 硬盘的存储单元个数的最大值
#define MAX_REQUEST_NUM (30000000 + 1) // 某一时间片内读取对象的总次数的最大值
#define MAX_OBJECT_NUM (100000 + 1)    // 某一时间片内删除和写入对象的总次数都保证小于等于100000
#define MAX_LABEL_NUM (16 + 1)         // 最大标签数
#define REP_NUM (3)                    // 副本数量
#define FRE_PER_SLICING (1800)
#define EXTRA_TIME (105)
// 在最后的105个时间片内，输入数据保证没有删除、写入和读取请求。

typedef struct Request_
{
    int object_id = 0;    // 该读取请求读取的对象的编号
    int prev_id = 0;      // 该读取请求读取的对象的上一次读取请求编号
    int phase = 0;        // 请求读取对象块的进度
    bool is_done = false; // 读取请求是否完成
    bool is_timeOut = false;
    vector<bool> is_read;  // 该读取请求读取了哪几个对象块
    int comeTimeStamp = 0; // 到来的时间戳
} Request;

typedef struct Object_
{
    int replica[REP_NUM + 1];
    vector<vector<int>> unit; // 改为vector<vector<int>>
    int size = 0;
    vector<int> readnum;
    int label = 0;
    int last_request_point;
    int last_request_time;
    bool is_delete;
    Object_()
    {
        last_request_point = 0;
        last_request_time = 0;
        is_delete = false;
        unit.resize(REP_NUM + 1); // 初始化unit vector
        for (int i = 0; i <= REP_NUM; i++)
        {
            replica[i] = 0;
        }
    }
} Object;

// 空闲区间结构
typedef struct FreeSegment_
{
    int start, end, label_value; // 修改成员变量名，避免和函数名冲突
    // label_value 代表该空闲区间的标签值
    //  构造函数
    FreeSegment_(int s, int e, int l) : start(s), end(e), label_value(l) {}

    // 成员函数 size()
    int size() const { return end - start + 1; }

    // 成员函数 label()
    int get_label() const { return label_value; } // 改名为 get_label() 以避免冲突
} FreeSegment;

// 磁盘状态结构
typedef struct DiskState_
{
    int head_position;  // 磁头位置
    int load;           // 当前负载（已使用的存储单元数）
    int tokens_left;    // 当前时间片剩余的令牌数
    int last_read_cost; // 上次读取操作的令牌消耗
    bool jumped;        // 本时间片是否已经执行过Jump操作

    DiskState_() : head_position(1), load(0), tokens_left(0), last_read_cost(64), jumped(false) {}
} DiskState;

Request request[MAX_REQUEST_NUM];    // 读取请求数组
Object object[MAX_OBJECT_NUM];       // 对象数组
DiskState disk_states[MAX_DISK_NUM]; // 磁盘状态数组
static int requestNum = 0;           // 还未完成的读取请求数量

int T, M, N, V, G;
// T代表本次数据有T+105个时间片 第T+1到T+105个时间片中。没有删除、写入、读取请求
// M代表对象标签数 1<=M<=16 N代表系统中硬盘的个数 V代表硬盘中的存储单元个数
// G代表每个磁头每个时间片最多消耗的令牌数

vector<vector<int>> disk(MAX_DISK_NUM, vector<int>(MAX_DISK_SIZE));
int disk_point[MAX_DISK_NUM];
// extern vector<int> position;            // 代表每个标签在硬盘中的前后顺序
vector<pair<int, int>> position(MAX_LABEL_NUM, {0, 0}); // 定义
int timestamp;                                          // 时间片记录
vector<vector<int>> frequent;
unordered_map<int, vector<FreeSegment>> free_segments[MAX_DISK_NUM]; // 每个磁盘每个标签的空闲区间
// 行是磁盘编号，列是每种标签的空闲区数组，这是一个map，并不是一个数组
random_device rd;
mt19937 rng(rd());

// 初始化空闲区间
void init_free_segments()
{
    // 遍历每个磁盘（从1到N）执行的都是相同的动作
    for (int i = 1; i <= N; i++)
    {
        free_segments[i].clear(); // 清空第i个磁盘的所有空闲区间数据

        // 遍历每个标签（从1到M）
        for (int tag = 1; tag <= M; tag++)
        {
            int start = 0;
            int end = 0;
            // 寻找标签tag在position数组中的位置
            for (int j = 1; j <= M; j++)
            {
                if (position[j].first == tag)
                { // tag标签应该放在硬盘上第j位

                    // 计算该标签对应的起始存储单元位置
                    // 例如：如果M=4，V=100，那么每个标签占25个单元
                    // j=1时，start=1
                    // j=2时，start=26
                    // j=3时，start=51
                    // j=4时，start=76
                    start = position[j].second;
                    end = position[(j + 1) % M == 0 ? M : (j + 1) % M].second - 1;
                    if ((j + 1) % M == 1)
                    {
                        end = V;
                    }
                    break;
                }
            }

            // 清空该磁盘该标签的空闲区间
            free_segments[i][tag].clear();

            free_segments[i][tag].push_back(FreeSegment(start, end, tag));
        }
    }
}

// 初始化磁盘空间
void init_disk_space()
{
    for (int i = 1; i <= N; i++)
    {
        fill(disk[i].begin(), disk[i].end(), 0);
        disk_point[i] = 1;
    }
}

void timestamp_action()
{
    scanf("%*s%d", &timestamp);
    // 这里%*s表示读取一个字符串但不保存它，%d表示读取一个整数并保存到timestamp变量中
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
}

// 合并指定磁盘和标签的相邻空闲区段
void merge_free_segments(int disk_id, int tag)
{
    // 如果该磁盘的该标签下没有空闲区段，直接返回
    if (free_segments[disk_id][tag].empty())
        return;

    // 按照起始地址排序，确保遍历时能顺序合并
    std::sort(free_segments[disk_id][tag].begin(), free_segments[disk_id][tag].end(),
              [](const FreeSegment &a, const FreeSegment &b)
              { return a.start < b.start; });

    std::vector<FreeSegment> merged;
    FreeSegment current = free_segments[disk_id][tag][0]; // 先取第一个区间作为当前合并区间
    // free_segments[disk_id][tag]是一个vector<FreeSegment>，里面存储的是FreeSegment类型的元素，所以free_segments[disk_id][tag][0]是第一个元素

    for (size_t i = 1; i < free_segments[disk_id][tag].size(); i++)
    {
        const FreeSegment &next = free_segments[disk_id][tag][i];

        if (current.end + 1 >= next.start)
        {
            // 如果当前区段的结束位置+1 >= 下一个区段的起始位置，说明可以合并
            current.end = std::max(current.end, next.end);
        }
        else
        {
            // 不能合并，当前区间为一个单独的空闲区，然后开始新的区间
            merged.push_back(current);
            current = next;
        }
    }

    // 最后一个区间加入到合并后的列表中
    merged.push_back(current);

    // 更新 free_segments[disk_id][tag]
    free_segments[disk_id][tag] = merged;
}

// 释放存储单元，更新空闲区间
void free_units(int disk_id, const std::vector<int> &units, int label)
{ // disk_id代表是磁盘数组中的哪一个磁盘
    // units代表这个磁盘中要释放的存储单元编号数组
    // label代表这个对象的标签
    for (int unit : units)
    {
        free_segments[disk_id][label].push_back(FreeSegment(unit, unit, label));
        // FreeSegment(空闲区开始地址，空闲区结束地址，空闲去的标签)是FreeSegment的构造函数
        // 这里是把每一个单独的存储单元作为了一个空闲区（因为释放这个对象的对象块占用的存储单元不一定是连续的）
    }
    merge_free_segments(disk_id, label); // 合并相邻的空闲区间
}

void do_object_delete(const vector<int> &object_unit, vector<int> &disk_unit, int size)
{
    // 删除在该副本磁盘上，该对象的各个对象块
    for (int i = 1; i <= size; i++)
    {
        disk_unit[object_unit[i]] = 0;
        // object_unit代表在该副本磁盘上，该对象的不同块的存储单元编号数组
        // disk_unit代表编号为id的对象的第j个副本磁盘
    }
}
// 删除事件交互
void delete_action()
{
    int n_delete;                   // 这一时间片内被删除的对象个数
    int abort_num = 0;              // 该时间片内被取消的读取请求的数量
    static int _id[MAX_OBJECT_NUM]; // 要被删除的对象的id数组

    scanf("%d", &n_delete);
    for (int i = 1; i <= n_delete; i++)
    {
        scanf("%d", &_id[i]);
    } // 判题器输入

    for (int i = 1; i <= n_delete; i++)
    {
        int id = _id[i];
        int current_id = object[id].last_request_point; // 获取该对象的最后一个请求编号
        while (current_id != 0)
        {
            if (request[current_id].is_done == false)
            {                // 如果该请求还没有完成
                abort_num++; // 被停止的读取请求数量加一
                requestNum--;
            }
            current_id = request[current_id].prev_id; // 获取该对象的上一个读取请求编号
            // request.prev_id初始化为0的
        }
    }

    printf("%d\n", abort_num); // 输出被停止的读取请求数量
    for (int i = 1; i <= n_delete; i++)
    {                                                   // 遍历每一个要被删除的对象
        int id = _id[i];                                // 获取其编号，这里n_delete<=数组_id[]的长度
        int current_id = object[id].last_request_point; // 获取该对象的最近一次读取请求编号
        while (current_id != 0)
        { // 遍历该对象的所有读取请求（不只是在这一个时间片内）
            if (request[current_id].is_done == false)
            {
                printf("%d\n", current_id); // 如果没有完成，就输出
            }
            current_id = request[current_id].prev_id; // 获取该读取请求读取的对象的上一次读取请求编号
        }
        for (int j = 1; j <= REP_NUM; j++)
        { // 遍历删除该对象的所有副本
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
            // object[id].unit[j] 编号为id的对象的各个块在它的第j个存储磁盘上的存储单元编号数组
            // disk[object[id].replica[j]] 编号为id的对象的第j个副本磁盘在所有磁盘数组中的编号
        }

        // 释放存储空间
        for (int j = 1; j <= REP_NUM; j++)
        {
            int disk_id = object[id].replica[j];  // 获取各副本存储磁盘在磁盘数组中的编号
            int current_label = object[id].label; // 获取这个对象的标签
            std::vector<int> units_to_free;       //

            for (int k = 1; k <= object[id].size; k++)
            {
                int unit = object[id].unit[j][k];
                disk[disk_id][unit] = 0;       // 在disk数组中，编号为disk_id的磁盘的第unit个存储单元被释放空间
                units_to_free.push_back(unit); // 将该存储单元编号加入到units_to_free数组中
            }

            free_units(disk_id, units_to_free, current_label); // 释放存储单元，更新空闲区间
            disk_states[disk_id].load -= object[id].size;      // 更新磁盘负载
        }
        object[id].is_delete = true; // 将该对象标记为已删除
    }

    fflush(stdout); // 强制刷新标准输出缓冲区，确保数据立即输出，不受缓冲机制影响
}

// 选择最佳磁盘进行写入
int select_best_disk(int object_id, int size, int tag, int replica_idx)
{ // object_id代表是对象数组中的哪一个对象
    // size代表是对象的大小
    // tag代表是对象的标签
    // replica_idx代表是对象的第几个副本

    // 这个对象已使用的磁盘集合
    bool used_disk[MAX_DISK_NUM] = {false};
    for (int j = 1; j < replica_idx; j++)
    {
        used_disk[object[object_id].replica[j]] = true;
    }
    // 置前replica_idx-1个副本的磁盘为已使用

    // 计算每个磁盘的得分
    std::vector<std::pair<double, int>> disk_scores;
    for (int i = 1; i <= N; i++)
    {
        if (used_disk[i])
            continue; // 如果该磁盘已使用，则跳过

        // 检查是否有足够的空间（连续或非连续）
        int total_free_space = 0;
        for (const auto &segment : free_segments[i][tag])
        { // 遍历第i个磁盘的该标签的每个空闲区
            total_free_space += segment.size();
        }
        // total_free_space代表第i个磁盘的该标签的空闲区总大小

        if (total_free_space < size)
            continue; // 如果该磁盘的该标签的空闲区总大小小于对象的大小，则跳过

        // 检查是否有足够大的连续空间
        bool has_continuous = false; // 是否有能够连续存储该对象的空闲区
        int best_segment_size = 0;   // 最佳连续空间大小
        for (const auto &segment : free_segments[i][tag])
        { // 遍历第i个磁盘的该标签的每个空闲区
            if (segment.size() >= size)
            {                          // 如果该空闲区的大小大于等于对象的大小
                has_continuous = true; // 有能够连续存储该对象的空闲区
                if (best_segment_size == 0 || segment.size() < best_segment_size)
                {
                    best_segment_size = segment.size(); // 更新最佳连续空间大小
                }
            }
            // 这部分代码其实是找这个磁盘中是否有能够连续存储该对象的空闲区，如果有，则找出最小的能够连续存储该对象的空闲区
        }

        // 计算得分 (多因素综合评分)
        double score = 3; // 初始化得分为3，保证没有足够连续的空余空间也能够有正分。

        // 1. 是否有能够连续存储该对象的空闲区因素 (连续空间越大越好)
        score += has_continuous ? 10.0 : 0.0; // 如果存在能够连续存储该对象的空闲区，则得分加10，否则得分加0

        // 2. 连续空间大小因素 (尽量选择刚好合适的连续空间)
        if (has_continuous)
        {
            score += 5.0 * (1.0 - (double)(best_segment_size - size) / V);
        }

        // 3. 负载均衡因素 (负载越低越好)
        score -= (double)disk_states[i].load / V * 3.0;

        disk_scores.push_back({score, i});
    }

    if (disk_scores.empty())
    {
        return 0;
    }

    // 选择得分最高的磁盘
    std::sort(disk_scores.begin(), disk_scores.end(),
              [](const std::pair<double, int> &a, const std::pair<double, int> &b)
              {
                  return a.first > b.first;
              }); // 按照得分从高到低排序

    return disk_scores[0].second; // 返回得分最高的磁盘编号
}

// 从空闲区间分配连续的存储单元（最佳适配算法）
bool allocate_continuous_segment(int disk_id, int size, std::vector<int> &allocated_units, int tag)
{
    int best_fit_size = V + 1;                            // 最佳适配算法，初始化最佳适配大小为V+1
    auto best_fit_it = free_segments[disk_id][tag].end(); // 初始化最佳适配迭代器为这个硬盘上该标签空闲区数组的最后一个空闲区的下一个（为空）

    for (auto it = free_segments[disk_id][tag].begin(); it != free_segments[disk_id][tag].end(); ++it)
    {
        if (it->size() >= size && it->size() < best_fit_size)
        {                               // 如果该空闲区的大小大于等于对象的大小，并且小于最佳适配大小
            best_fit_size = it->size(); // 更新最佳适配大小
            best_fit_it = it;           // 更新最佳适配迭代器
        }
    }

    if (best_fit_it != free_segments[disk_id][tag].end())
    {                                   // 如果最佳适配迭代器不是最后一个空闲区的下一个（空）
        int start = best_fit_it->start; // 获取最佳适配空闲区的起始地址
        for (int i = 0; i < size; i++)
        {
            allocated_units.push_back(start + i); // 将最佳适配空闲区中要分配给这个对象的存储单元编号加入到allocated_units数组中
        }

        // 更新空闲区间
        if (best_fit_it->size() == size)
        {
            free_segments[disk_id][tag].erase(best_fit_it); // 如果最佳适配空闲区的大小正好等于对象的大小，则删除这个空闲区
        }
        else
        {
            best_fit_it->start += size; // 如果最佳适配空闲区的大小大于对象的大小，则更新最佳适配空闲区的起始地址
        }

        return true; // 返回true，表示分配成功
    }

    return false; // 返回false，表示分配失败
}

// 分配存储单元（尝试连续分配，如果失败则进行非连续分配）
void allocate_storage_units(int disk_id, int size, std::vector<int> &allocated_units, int tag)
{ // disk_id代表是磁盘数组中的哪一个磁盘
    // size代表是对象的大小
    // tag代表是对象的标签
    // allocated_units代表是存储单元编号数组（待填入）

    // 尝试连续分配
    bool allocated = allocate_continuous_segment(disk_id, size, allocated_units, tag);

    // 如果连续分配失败，进行非连续分配
    if (!allocated)
    {
        // 收集所有空闲存储单元
        std::vector<int> all_free_units;
        for (const auto &segment : free_segments[disk_id][tag])
        {
            for (int i = segment.start; i <= segment.end; i++)
            {
                all_free_units.push_back(i);
            }
        }

        // 选择前size个单元
        for (int i = 0; i < size && i < all_free_units.size(); i++)
        {
            allocated_units.push_back(all_free_units[i]); // 将空闲存储单元编号加入到allocated_units数组中
        }

        // 更新空闲区间
        for (int unit : allocated_units)
        { // 遍历要分配给这个对象的存储单元
            for (auto it = free_segments[disk_id][tag].begin(); it != free_segments[disk_id][tag].end();)
            { // 遍历这个硬盘上该标签的每个空闲区
                if (it->start <= unit && unit <= it->end)
                { // 如果有某一个被分配的存储单元落在这个空闲区中

                    if (it->start == unit && it->end == unit)
                    {
                        // 整个区间被使用
                        it = free_segments[disk_id][tag].erase(it);
                    }
                    else if (it->start == unit)
                    {
                        // 区间起始被使用
                        it->start++;
                        ++it;
                    }
                    else if (it->end == unit)
                    {
                        // 区间结束被使用
                        it->end--;
                        ++it;
                    }
                    else
                    {
                        // 区间中间被使用，需要分割
                        int old_end = it->end;
                        it->end = unit - 1; // 原来的空闲区结束地址更新为被分配的存储单元的前一个存储单元
                        free_segments[disk_id][tag].push_back(FreeSegment(unit + 1, old_end, tag));
                        // 新的空闲区起始地址为被分配的存储单元的下一个存储单元，结束地址为原来的空闲区结束地址，标签为原来的空闲区标签
                        ++it;
                    }
                    break; // 这个存储单元所在的空闲区更新完毕，跳出循环，处理下一个存储单元
                }
                else
                { // 如果被分配的存储单元落在这个空闲区之外，则跳过
                    ++it;
                }
            }
        }
    }
}

// 处理写入请求
void write_action()
{
    int n_write;
    cin >> n_write;
    vector<int> written_ids(n_write + 1); // 保存写入的对象ID
    for (int i = 1; i <= n_write; i++)
    {
        int id, size, label;
        cin >> id >> size >> label;
        written_ids[i] = id; // 保存写入的对象ID

        // 输入写入对象相关信息
        object[id].size = size;
        object[id].label = label;
        object[id].readnum.resize(size + 1, 0); // 使用vector的resize方法
        object[id].last_request_point = 0;
        object[id].is_delete = false;

        for (int j = 1; j <= REP_NUM; j++)
        {
            object[id].unit[j].resize(size + 1); // 使用resize替代malloc
        }

        // 为每个副本选择磁盘并分配存储单元
        for (int j = 1; j <= REP_NUM; j++)
        {

            int disk_id; // 选择一个磁盘存放该对象的副本
            std::vector<int> allocated_units;
            int k;
            for (k = 1; k <= M; k++)
            {
                if (position[k].first == label)
                {
                    break;
                }
            }
            while (k <= M)
            {
                int current_label = position[k].first;
                disk_id = select_best_disk(id, size, current_label, j);
                if (!free_segments[disk_id][current_label].empty() || disk_id != 0)
                {
                    object[id].replica[j] = disk_id; // 记录该对象的副本存储在磁盘数组中的编号

                    allocate_storage_units(disk_id, size, allocated_units, current_label); // 分配存储单元
                    break;
                }
                k++;
                if (k > M)
                {
                    k = k % M;
                }
            }

            // 更新对象信息和磁盘状态
            for (int k = 0; k < size; k++)
            {
                int unit = allocated_units[k];
                object[id].unit[j][k + 1] = unit; // 记录该对象的第j个副本的第k+1个对象块存储的存储单元编号
                disk[disk_id][unit] = id;         // 在disk数组中，编号为disk_id的磁盘的第unit个存储单元存储了编号为id的对象
                object[id].readnum[k + 1] = 0;    // 该对象的第k+1个对象块待读取次数初始化为0
            }

            // 更新磁盘负载
            disk_states[disk_id].load += size; // 更新磁盘负载
        }
    }
    // 输出写入结果
    for (int i = 1; i <= n_write; i++)
    {
        int obj_id = written_ids[i]; // 获取本时间片内的写入对象id
        printf("%d\n", obj_id);      // 输出对象编号

        for (int j = 1; j <= REP_NUM; j++)
        {
            printf("%d", object[obj_id].replica[j]); // 输出该对象的第j个副本的存储磁盘编号
            for (int k = 1; k <= object[obj_id].size; k++)
            {
                printf(" %d", object[obj_id].unit[j][k]); // 输出该对象的各个对象块在第j个副本磁盘上存储的存储单元编号
            }
            printf("\n");
        }
    }

    if (n_write > 0)
    {
        fflush(stdout);
    }
}

// 计算窗口得分
double calculate_window_score(int start, int disk_id, vector<int> current_frequent_position,char lastAction,int lastConsume)
{
    int TIME_WINDOW = 90; // 只考虑最近TIME_WINDOW个时间片内的请求
    //int WINDOW_SIZE = 0;  // 滑动窗口大小
    double score = 0;
    int consume = 0;
    int p = start;
    while (consume <= G)
    {
        int object_id = disk[disk_id][p]; // 获取该位置是哪个对象
        int recall_id = object[object_id].last_request_point;

        // 寻找是这个对象的第几个对象块
        bool block_found = false;
        int order = 0; // 这个对象的第order个对象块
        for (int k = 1; k <= REP_NUM; k++)
        {
            if (object[object_id].replica[k] == disk_id)
            {
                // 是当前磁盘
                for (int j = 1; j <= object[object_id].size; j++)
                {
                    if (object[object_id].unit[k][j] == disk_point[disk_id])
                    {
                        block_found = true;
                        order = j;
                        break; // 找到了就退出
                    }
                }
                if (block_found)
                    break; // 找到了就退出
            }
            else
            {
                // 不是当前磁盘就找下一个副本磁盘
            }
        }

        // 从当前请求的编号回溯，找到为哪个请求读取这个对象块
        while (recall_id>0)
        {
            if (request[recall_id].is_read[order] == false)
            { // 当前请求还未读取该对象块
                break;
            }
            else
            { // 当前请求已读取该对象块
                recall_id = request[recall_id].prev_id;
            }
        }
        // 最终recall_id代表最近的还没有读取这个对象块的请求编号

        if (timestamp - request[recall_id].comeTimeStamp > 90 || request[recall_id].comeTimeStamp == 0 || object_id == 0||recall_id==0)
        {
            //请求超时或者当前单元没有对象块 pass
            if (consume+1<=G)
            {
                //记录消耗、这一步的动作和令牌消耗
                consume = consume + 1;
                lastConsume = 1;
                lastAction = 'p';
            }else { break;}
        }
        else
        {
            Request req = request[recall_id];
            int delta_time = timestamp - req.comeTimeStamp;
            int object_id = req.object_id;

            if (object[object_id].readnum[order] > 0) {
                //记录消耗、这一步的动作和令牌消耗
                if (lastAction=='r'){
                    if (consume+ max(16, static_cast<int>(lastConsume * 0.8) + 1)<=G){
                        consume += max(16, static_cast<int>(lastConsume * 0.8) + 1);
                        lastConsume = max(16, static_cast<int>(lastConsume * 0.8) + 1);
                        lastAction = 'r';
                    }else{break;}
                }else{
                    if (consume+64<=G){
                        consume += 64;
                        lastConsume = 64;
                        lastAction = 'r';
                    }else{break;}
                }

                //增加这一次的得分
                if (delta_time < 10) {
                    score += (1 - 0.005 * delta_time) * (1 + 1.0/ object[object_id].size) * 0.5;
                }
                else {
                    score += (1.05 - 0.01 * delta_time) * (1 + 1.0 / object[object_id].size) * 0.5;
                }
            }
            else {
                //不需要读 则没分
                if (consume+1<G){
                    //记录消耗、这一步的动作和令牌消耗
                    consume = consume + 1;
                    lastConsume = 1;
                    lastAction = 'p';
                }else { break;}
            }
        }
        p++;
        if (p>V)
        {
            p = p % V;
        }
    }
    p--;
    if (p==0)
    {
        p = V;
    }
    //设置滑动窗口的大小
    /*WINDOW_SIZE = p - start;*/

    for (int current_position : current_frequent_position) {
        //遍历每一个频繁标签的位置
        int frequent_start = position[current_position].second;
        int frequent_end = position[current_position + 1].second;
        if (p >= frequent_start && p < frequent_end) {
            //如果当前磁头在某一个频繁标签的范围内（左闭右开）
            score *= 1.2;//窗口尾部处于频繁标签范围，认为它更有潜力在之后继续得到高分
        }
    }

    return score;
}

// 读取事件交互
void read_action()
{

    int n_read; // 这一时间片读取对象的个数
    int request_id, object_id;
    scanf("%d", &n_read);
    requestNum += n_read; // 增加requestNum

    // 初始化
    for (int i = 1; i <= n_read; i++)
    {                                           // 对于每一个要读取请求
        scanf("%d%d", &request_id, &object_id); // 获取请求编号和请求的对象编号

        request[request_id].object_id = object_id;                          // 设置该请求的请求对象编号
        request[request_id].prev_id = object[object_id].last_request_point; // 记录 该读取请求读取的对象的上一次读取请求编号
        request[request_id].phase = 0;                                      // 请求读取对象块的进度
        request[request_id].is_done = false;                                // 该读取请求未完成
        request[request_id].comeTimeStamp = timestamp;                      // 到来的时间戳
        // 修改初始化方式，确保vector大小足够且安全
        request[request_id].is_read.clear();                                   // 先清空
        request[request_id].is_read.resize(object[object_id].size + 1, false); // 重新分配大小并初始化
        // 读取请求初始化完毕

        object[object_id].last_request_point = request_id; // 这个对象的上一次读取请求编号置为当前读取请求的编号
        object[object_id].last_request_time = timestamp;   // 记录当前对象最新请求时间
        for (int i = 1; i <= object[object_id].size; i++)
        {
            object[object_id].readnum[i]++; // 这个对象的对象块待读取次数加1
        }
        // 读取的对象更新完毕
    }
    // 读取请求数组初始化完成

    static int current_request = 0;            // 记录当前正在处理的请求编号
    static char *last_act = new char[N + 1];   // last_act[i]记录第i个磁头的上一个动作
    static int *last_consume = new int[N + 1]; // last_consume[i]记录第i个磁头的上一个动作的令牌消耗
    static bool first_call = true;             // 是否是第一次调用read_action()，是否是第一个时间片

    if (first_call)
    {
        for (int i = 1; i <= N; i++)
        {
            last_act[i] = ' '; // 初始化为空
        }
        first_call = false;
    }

    if (!current_request && n_read > 0)
    {
        // 只有在这个时间片有读请求（n_read > 0）并且 没有之前时间片的读请求正在执行（current_request为0）
        current_request = request_id - n_read + 1; // 初始化current_request 为当前时间片的第一个读取请求编号
        // request_id 经过for循环后指向的是最后一个读取请求的编号，减去n_read再+1后就是...
    }

    if (!current_request)
    {
        // 若 current_request 仍然为空 则当前时间片没有读取请求
        for (int i = 1; i <= N; i++)
        {
            printf("#\n"); // 磁头的运动输出为"#"
        }
        printf("0\n"); // 这个0是n_rsp，是该时间片内上报成功的读取请求数目
    }
    else
    {                                        // 有读取请求
        int n_rsp = 0;                       // 该时间片内完成的读取请求数量
        vector<int> timeOut(N + 1, 0);       // 某一磁头的时间片内令牌数是否足以支撑下一个动作，0代表可以支撑，1代表不足以支撑
        int timeOut_num = 0;                 // 停下的磁头数量
        int *consume = new int[N + 1];       // 各个磁头消耗本时间片内令牌的数量
        string *content = new string[N + 1]; // 各个磁头在本时间片内的移动轨迹
        std::vector<int> successRequest;     // 本时间片内上报成功的读取请求编号

        for (int i = 1; i <= N; i++)
        {
            consume[i] = 0; // 初始化为0
            content[i] = "";
        }

        // 寻找当前时间点的频繁标签
        int time_period = timestamp / 1800 + 1; // 获取当前时间段
        vector<int> current_frequent_position;  // 当前时间段内频繁标签的位置
        for (int i = 1; i <= M; i++)
        { // 遍历每一种标签
            if (frequent[i][time_period] != 0)
            { // 如果该标签在当前时间段内是频繁的
                for (int j = 1; j <= M; j++)
                { // 在position数组中寻找该标签的顺序
                    if (position[j].first == i)
                    {                                           // 如果第i个标签在第j个位置
                        current_frequent_position.push_back(j); // 把该标签的顺序j加入到current_frequent_position数组中
                        break;
                    }
                }
            }
        }
        // 现在current_frequent_position数组中存储了当前时间段内频繁标签的位置
        // 对current_frequent_position数组进行排序
        if (!current_frequent_position.empty())
        {
            sort(current_frequent_position.begin(), current_frequent_position.end());
        }

        bool start_time = true; // 是否是一个时间片的最开始
        while (timeOut_num < N)
        { // 只要还有没停下的磁头，就继续循环执行
            for (int i = 1; i <= N; i++)
            { // 遍历每一个磁盘的磁头

                // 如果是一个时间片的最开始，则需要判断是否需要jump
                if (start_time)
                {
                    start_time = false;
                    if (n_read > 50)
                    {                        // 选择频繁标签
                        bool is_jump = true; // 是否需要jump
                        if (current_frequent_position.empty())
                        {
                            is_jump = false;
                        }
                        else
                        {
                            for (int current_position : current_frequent_position)
                            { // 遍历每一个频繁标签的位置
                                int start = position[current_position].second;
                                int end = position[current_position + 1].second + G;
                                if (disk_point[i] >= start && disk_point[i] < end)
                                {                    // 如果当前磁头在某一个频繁标签的范围内（左闭右开）
                                    is_jump = false; // 第i个磁头可以读取到频繁标签，则不必Jump，
                                    break;           // 不必再为这个磁头探寻其他频繁标签范围
                                }
                            }
                        }

                        if (is_jump)
                        {
                            // 第i个磁头不在任何频繁标签的范围内，则需要jump
                            int mark = 0;
                            int jump_area;
                            for (int current_position : current_frequent_position)
                            { // 找距离当前磁头最近的频繁标签范围 上面current_frequent_position已经按照顺序排序了
                                if (disk_point[i] < position[current_position].second)
                                {                                                      // 如果磁头在某一频繁标签范围的左侧 因为磁头只能向右移动，所以只能跳到在磁头右边的频繁标签范围
                                    disk_point[i] = position[current_position].second; // 跳到该频繁标签范围的左端
                                    jump_area = position[current_position].second;
                                    mark = 1; // 标记找到了要跳的频繁标签范围
                                    break;
                                }
                            }
                            if (mark == 0)
                            {                                                                  // 如果磁头在所有频繁标签范围的右侧，则跳到第一个频繁标签范围的左端
                                disk_point[i] = position[current_frequent_position[0]].second; // current_frequent_position是用0位置的
                                jump_area = position[current_frequent_position[0]].second;
                            }
                            last_act[i] = 'j';                 // 设置这个磁头的上一次动作是jump
                            last_consume[i] = G;               // 设置这个磁头上一次消耗的令牌数为G
                            consume[i] = consume[i] + G;       // 这个磁头本时间片内消耗的令牌数加G
                            timeOut[i] = 1;                    // 这个磁头的时间片内令牌数消耗完
                            timeOut_num++;                     // 停下的磁头数量加1
                            content[i] = to_string(jump_area); // 把jump动作加入到content[i]中
                            // 题目要求jump时，输出两个字符串，第一个字符串固定是 "j"，第二个字符串表示跳跃到的存储单元编号
                            // 为了和下面输出时一致，这里仅记录跳跃到的存储单元编号，在下面输出"j"
                        }
                    }
                    else
                    {
                        // 全局最优选择
                        vector<pair<int, double>> window_scores;
                        //const int STEP = 50; // 窗口滑动步长
                        //for (int start = 1; start <= V; start += STEP)
                        //{
                        //    double score = calculate_window_score(start, i, current_frequent_position,last_act[i],last_consume[i]);
                        //    window_scores.emplace_back(start, score);
                        //}
                        
                        int start = disk_point[i];
                        int camp = start;
                        bool flag = false;
                        while (true) {
                            int step = 1;
                            int end = start + step;
                            while (step<V) {
                                int endobject_id = disk[i][end];
                                if (endobject_id == 0 || object[endobject_id].is_delete) {
                                    step++;
                                }
                                else
                                {
                                    // 寻找是这个对象的第几个对象块
                                    bool block_found = false;
                                    int order = 0; // 这个对象的第order个对象块
                                    for (int k = 1; k <= REP_NUM; k++)
                                    {
                                        if (object[object_id].replica[k] == i)
                                        {
                                            // 是当前磁盘
                                            for (int j = 1; j <= object[object_id].size; j++)
                                            {
                                                if (object[object_id].unit[k][j] == disk_point[i])
                                                {
                                                    block_found = true;
                                                    order = j;
                                                    break; // 找到了就退出
                                                }
                                            }
                                            if (block_found)
                                                break; // 找到了就退出
                                        }
                                        else
                                        {
                                            // 不是当前磁盘就找下一个副本磁盘
                                        }
                                    }

                                    if (object[endobject_id].readnum[order]>0)
                                    {
                                        break;
                                    }
                                    else
                                    {
                                        step++;
                                    }
                                }
                            }
                            double score = calculate_window_score(start, i, current_frequent_position,last_act[i],last_consume[i]);
                            window_scores.emplace_back(start, score);
                            if (start+step>V)
                            {
                                flag = true;
                                start = (start + step) % V;
                            }
                            if (flag&&start>=camp)
                            {
                                break;
                            }
                        }

                        // 选择所有窗口中得分最高的一组
                        auto best = max_element(window_scores.begin(), window_scores.end(),
                                                [](auto &a, auto &b)
                                                { return a.second < b.second; });

                        double current_score = calculate_window_score(disk_point[i], i, current_frequent_position, last_act[i],last_consume[i]);
                        if (best != window_scores.end() && best->second > current_score * 2)
                        {
                            disk_point[i] = best->first;         // 更新磁头位置
                            last_act[i] = 'j';                   // 设置这个磁头的上一次动作是jump
                            last_consume[i] = G;                 // 设置这个磁头上一次消耗的令牌数为G
                            consume[i] = consume[i] + G;         // 这个磁头本时间片内消耗的令牌数加G
                            timeOut[i] = 1;                      // 这个磁头的时间片内令牌数消耗完
                            timeOut_num++;                       // 停下的磁头数量加1
                            content[i] = to_string(best->first); // 把jump动作加入到content[i]中
                        }
                    }
                }

                if (timeOut[i] == 0)
                { // 如果第i个磁头还没有停下

                    object_id = disk[i][disk_point[i]]; // 获取当前磁盘的磁头所在的存储单元所存储的对象编号

                    // 当前存储单元没有存储对象块，或对象已被删除：停下或pass
                    if (object_id == 0 || object[object_id].is_delete)
                    {
                        if (requestNum == 0)
                        {
                            // 当前没有需要读取的请求,磁头停下
                            timeOut[i] = 1; // 这个磁头停下
                            timeOut_num++;  // 停下的磁头数量加1
                            content[i] = content[i] + "#";
                            continue; // 这个磁头这次的执行结束，跳到下一个磁头
                        }
                        else
                        { // 当前还有要被读取的请求，pass
                            if (consume[i] + 1 <= G)
                            {
                                // 能够pass
                                last_act[i] = 'p';
                                consume[i] = consume[i] + 1;
                                last_consume[i] = 1;
                                content[i] = content[i] + 'p';
                                disk_point[i] = (disk_point[i] % V) + 1;
                                continue; // 这个磁头这次的执行结束，跳到下一个磁头
                            }
                            else
                            {
                                // 不能够pass
                                timeOut[i] = 1; // 这个磁头的时间片内令牌数消耗完
                                timeOut_num++;  // 停下的磁头数量加1
                                content[i] = content[i] + "#";
                                continue; // 这个磁头这次的执行结束，跳到下一个磁头
                            }
                        }
                    }

                    // 当前存储单元有存储对象块，且对象未被删除

                    // 寻找是这个对象的第几个对象块
                    bool block_found = false;
                    int order = 0; // 这个对象的第order个对象块
                    for (int k = 1; k <= REP_NUM; k++)
                    {
                        if (object[object_id].replica[k] == i)
                        {
                            // 是当前磁盘
                            for (int j = 1; j <= object[object_id].size; j++)
                            {
                                if (object[object_id].unit[k][j] == disk_point[i])
                                {
                                    block_found = true;
                                    order = j;
                                    break; // 找到了就退出
                                }
                            }
                            if (block_found)
                                break; // 找到了就退出
                        }
                        else
                        {
                            // 不是当前磁盘就找下一个副本磁盘
                        }
                    }

                    if (object[object_id].readnum[order] > 0 && timestamp - object[object_id].last_request_time <= 90)
                    { // 如果该对象块还需要读取

                        // 从当前请求的编号回溯，找到为哪个请求读取这个对象块
                        int recall_id = object[object_id].last_request_point;
                        int prev_id = request[request_id].prev_id;
                        while (true)
                        {
                            if (request[recall_id].is_read[order] == false)
                            { // 当前请求还未读取该对象块
                                break;
                            }
                            else
                            { // 当前请求已读取该对象块
                                recall_id = request[recall_id].prev_id;
                            }
                        }
                        // 最终recall_id代表当前是为哪个编号的请求读取这个对象块

                        // 读取
                        // 磁头执行的首个动作，并且是读
                        if (last_act[i] == ' ')
                        {
                            last_act[i] = 'r';
                            last_consume[i] = 64;
                            consume[i] = consume[i] + 64;
                            content[i] = content[i] + 'r';

                            disk_point[i] = (disk_point[i] % V) + 1; // 修改为当前磁头

                            object[object_id].readnum[order] = object[object_id].readnum[order] - 1;

                            request[recall_id].phase++;
                            request[recall_id].is_read[order] = true;

                            if (request[recall_id].phase == object[object_id].size)
                            { // 这个请求已经读取完毕
                                request[recall_id].is_done = true;
                                successRequest.push_back(recall_id);
                                n_rsp++;
                                requestNum--; // 待读取的请求数量减少
                            }
                        }
                        else if (last_act[i] == 'r')
                        {                                                                            // 非第一个时间片的第一个动作，且上一个动作是读
                            int next_consume = max(16, static_cast<int>(last_consume[i] * 0.8) + 1); // 向上取整

                            if (consume[i] + next_consume <= G)
                            { // 足以支撑这一次读动作
                                last_act[i] = 'r';
                                consume[i] = consume[i] + next_consume;
                                last_consume[i] = next_consume;
                                content[i] = content[i] + 'r';
                                object[object_id].readnum[order] = object[object_id].readnum[order] - 1;
                                disk_point[i] = (disk_point[i] % V) + 1; // 修改为当前磁头

                                request[recall_id].phase++;
                                request[recall_id].is_read[order] = true;

                                if (request[recall_id].phase == object[object_id].size)
                                { // 这个请求已经读取完毕
                                    request[recall_id].is_done = true;
                                    successRequest.push_back(recall_id);
                                    n_rsp++;
                                    requestNum--; // 待读取的请求数量减少
                                }
                            }
                            else
                            {
                                // 令牌不足，不能读，停在原地
                                timeOut[i] = 1; // 修改为当前磁头
                                timeOut_num++;
                                content[i] = content[i] + "#";
                            }
                        }
                        else if (last_act[i] == 'p' || last_act[i] == 'j') // 修改判断条件
                        {                                                  // 非第一个时间片的第一个动作,且上一个动作是pass或者jump
                            if (consume[i] + 64 <= G)
                            {

                                last_act[i] = 'r';
                                consume[i] = consume[i] + 64;
                                last_consume[i] = 64;
                                content[i] = content[i] + 'r';
                                object[object_id].readnum[order] = object[object_id].readnum[order] - 1;
                                disk_point[i] = (disk_point[i] % V) + 1; // 修改为当前磁头

                                request[recall_id].phase++;
                                request[recall_id].is_read[order] = true;
                                if (request[recall_id].phase == object[object_id].size)
                                { // 这个请求已经读取完毕
                                    request[recall_id].is_done = true;
                                    successRequest.push_back(recall_id);
                                    n_rsp++;
                                    requestNum--; // 待读取的请求数量减少
                                }
                            }
                            else
                            {
                                // 令牌不足，不能读，停在原地
                                timeOut[i] = 1; // 修改为当前磁头
                                timeOut_num++;
                                content[i] = content[i] + "#";
                            }
                        }
                    }
                    else
                    { // 这个对象块不用再读
                        if (consume[i] + 1 <= G)
                        { // 可以pass
                            if (requestNum == 0)
                            {
                                // 当前没有待读取的请求，磁头不必pass
                                timeOut[i] = 1; // 这个磁头的时间片内令牌数消耗完
                                timeOut_num++;  // 停下的磁头数量加1
                                content[i] = content[i] + "#";
                            }
                            else
                            { // 还有待读取的请求，pass
                                last_act[i] = 'p';
                                consume[i] = consume[i] + 1;
                                last_consume[i] = 1;
                                content[i] = content[i] + 'p';
                                disk_point[i] = (disk_point[i] % V) + 1; // 到下一个存储单元
                            }
                        }
                        else
                        { // 令牌不足，不能pass，停在原地
                            timeOut[i] = 1;
                            timeOut_num++;
                            content[i] = content[i] + "#";
                        }
                    }
                }
                // 这个磁头执行了一个动作，换下一个磁头
            } // 磁头依次执行动作
            start_time = false;
        }
        // 如果所有磁头都停下了，则停止循环执行

        // 输出
        for (int i = 1; i <= N; i++)
        {
            if (content[i].back() == '#')
            {
                // 以#结尾，表明不是jump
                cout << content[i] << endl; // 输出每个磁头在这个时间片内的动作
            }
            else
            {
                // 是jump
                cout << "j" << " " << content[i] << endl;
            }
        }
        cout << n_rsp << endl; // 输出本时间片上报成功的读取请求
        for (int i = 0; i < n_rsp; i++)
        {
            cout << successRequest[i] << endl; // 输出本时间片上报读取完成的读取请求编号
        }
    }
    fflush(stdout);
}

void clean()
{
    for (auto &obj : object)
    {
        for (int i = 1; i <= REP_NUM; i++)
        {
            obj.unit[i].clear(); // 清空vector
        }
    }
}

// 提取频繁标签
vector<vector<int>> extractFrequentTags(vector<vector<int>> &read_num, double percentage = 0.85)
{
    int M = read_num.size();                            // 标签个数+1
    int T = read_num[0].size();                         // 1800时间片个数+1
    vector<vector<int>> frequent(M, vector<int>(T, 0)); // 初始化为0

    for (int t = 1; t < T; ++t)
    {
        int max_val = 0;
        for (int i = 1; i < M; ++i)
        {
            max_val = max(max_val, read_num[i][t]);
        } // 找到某一时间段内，读取次数最多的标签的读取次数
        int threshold = max_val * percentage; // 以这个次数为频繁与否的标准，记录读取次数大于等于这个次数的标签为频繁标签

        for (int i = 0; i < M; ++i)
        {
            if (read_num[i][t] >= threshold)
            {
                frequent[i][t] = read_num[i][t]; // 记录t这个时间段内，读取次数大于等于threshold的标签的读取次数
                // 0.85是阈值，表示读取次数大于等于这个次数的标签为频繁标签
                // 小于这个次数的标签为非频繁标签，已初始化为0
            }
        }
    }
    return frequent;
}

// 计算余弦相似度
double cosineSimilarity(const vector<int> &a, const vector<int> &b)
{
    double dot_product = 0, norm_a = 0, norm_b = 0;
    for (size_t i = 0; i < a.size(); ++i)
    {                                             // a[i] 和 b[i] 是两个频繁标签在第i个时间片内的读取次数
        dot_product += a[i] * 0.01 * b[i] * 0.01; // 计算两个向量的点积
        norm_a += a[i] * 0.01 * a[i] * 0.01;      // 计算向量a的模
        norm_b += b[i] * 0.01 * b[i] * 0.01;      // 计算向量b的模
        // 0.01 是归一化因子，将读取次数归一化到[0,1]范围内
    }
    if (norm_a == 0 || norm_b == 0)
        return 0;                                       // 避免除零
    return dot_product / (sqrt(norm_a) * sqrt(norm_b)); // 计算并返回余弦相似度
}

// 计算相似性矩阵
vector<vector<double>> computeSimilarityMatrix(const vector<vector<int>> &frequent)
{
    int M = frequent.size();                                    // 标签个数+1
    vector<vector<double>> similarity(M, vector<double>(M, 0)); // 相似度矩阵

    for (int i = 1; i < M; ++i)
    {
        for (int j = i + 1; j < M; ++j)
        {
            similarity[i][j] = similarity[j][i] = cosineSimilarity(frequent[i], frequent[j]);
            // 利用余弦相似度计算两个频繁标签的相似度
            // frequent[i] 和 frequent[j] 是两个频繁标签在各个时间片内的读取次数
            // similarity[i][j] 和 similarity[j][i] 是两个频繁标签的相似度，similarity是对称矩阵
        }
    }
    return similarity;
}

// 层次聚类
vector<int> clusterLabels(const vector<vector<double>> &similarity, double threshold)
{
    int M = similarity.size();
    vector<int> cluster(M);                  // 初始化一个大小为M的向量，用于存储每个标签的聚类结果
    iota(cluster.begin(), cluster.end(), 0); // 使用iota函数将向量初始化为[0,1,2,...,M-1]

    for (int i = 1; i < M; ++i)
    {
        for (int j = i + 1; j < M; ++j)
        { // 遍历 任意两个标签（不区分顺序）
            if (similarity[i][j] > threshold)
            {                                 // 如果两个标签的相似度大于阈值
                int old_cluster = cluster[j]; // 记录标签j的聚类结果
                for (int k = 1; k < M; ++k)
                { // 遍历 所有标签
                    if (cluster[k] == old_cluster)
                        cluster[k] = cluster[i]; // 将所有属于标签J的簇的标签重新分配到标签i所在的簇
                }
            }
        }
    }
    // 这是一种基于相似度的层次聚类方法，通过不断合并相似度高的标签来构建聚类结果
    //  最终返回一个大小为M的向量，用于存储每个标签的聚类结果
    //  具有传递性：如果A和B相似，B和C相似，那么A和C会被分到同一簇
    return cluster;
}

// 判断因果关系
bool hasCausalRelation(const vector<int> &groupA, const vector<int> &groupB, const vector<vector<int>> &frequent)
{
    int time_len = frequent[0].size();
    double sumA = 0, sumB = 0;

    for (int t = 0; t < time_len; ++t)
    {
        double avgA = 0, avgB = 0;
        for (int id : groupA)
            avgA += frequent[id][t];
        for (int id : groupB)
            avgB += frequent[id][t];
        sumA += avgA;
        sumB += avgB;
    }
    return sumA > sumB;
}

vector<pair<int, int>> generateGlobalOrder(const vector<vector<int>> &grouped_labels, const vector<vector<int>> &frequent, vector<int> max)
{
    int group_count = grouped_labels.size();

    // 计算每个 group 的最早出现时间
    vector<pair<int, int>> group_time; // (最早出现时间, group_index)
    vector<int> start_position;
    // group_time是一个vector，每个元素是一个pair，每个pair包含两个类型的元素
    // 第一个元素是int类型，表示最早出现的时间
    // 第二个元素是int类型，表示group的index

    for (int i = 1; i <= group_count; ++i)
    { // 遍历每一种标签分组
        int min_time = 60;
        for (int label : grouped_labels[i - 1])
        {
            // 遍历每一种标签
            for (int j = 1; j < frequent[label].size(); ++j)
            { // 从早到晚遍历每个时间段
                if (frequent[label][j] > 0)
                {                                // 如果该标签在第j个时间段内要被读取
                    min_time = min(min_time, j); // 更新最早出现的时间
                    break;                       // 只记录最早出现的时间
                }
            }
            // 这个标签的最早获取时间已经找到，继续循环找这个标签组的其他标签的最早出现时间
        }
        // 这个标签组的最早出现时间已经找到，继续循环找下一个标签组的最早出现时间
        group_time.emplace_back(min_time, i);
    }
    // 所有标签组的最早出现时间已经找到
    //  按最早出现时间排序
    sort(group_time.begin(), group_time.end()); // sort会按照pair的第一个元素（最早出现时间）进行排序，如果第一个元素相同，则按照第二个元素（group的index）进行排序

    // 按排序后的顺序合并 group
    vector<pair<int, int>> final_order(M + 1); // 用1-16
    int i = 1;                                 // 从1开始填写final_order
    for (const pair<int, int> &p : group_time)
    { // 按 最早出现时间 将每个标签组 合并到final_order中
        int time = p.first;
        int idx = p.second;
        for (const int label : grouped_labels[idx - 1])
        {
            final_order[i].first = label;
            i++;
        }
        // 将第idx个标签组的各个标签 依次写入final_order的末尾final_order.end()
    }

    int sum = 0;
    for (int i = 1; i <= M; i++)
    {
        sum += max[i];
    }

    for (int i = 0; i < M; i++)
    {
        final_order[i + 1].second = max[final_order[i].first] * V / sum + final_order[i].second + 1;
    }

    return final_order;
}

// 初始化
void Init(vector<vector<int>> &read_num, int num_disks, vector<int> max)
{

    if (M == 0)
        return;

    // 1. 提取频繁标签
    frequent = extractFrequentTags(read_num, 0.85);

    // 2. 计算相似性矩阵
    vector<vector<double>> similarity = computeSimilarityMatrix(frequent);

    // 3. 聚类
    double threshold = 0.5;
    vector<int> cluster = clusterLabels(similarity, threshold);

    // 4. 整理分组
    vector<vector<int>> grouped_labels;
    // 存储每个标签的聚类结果
    // 例如：grouped_labels[0] = {1, 2, 3} 表示标签1、2、3属于同一簇
    vector<int> assigned(M + 1, -1); // 初始化一个大小为M+1的向量，用于存储每个标签的聚类结果，初始化为-1
    int group_id = 1;                // 初始化聚类id为1

    for (int i = 1; i <= M; ++i)
    {
        if (assigned[i] == -1)
        {
            vector<int> new_group;
            for (int j = 1; j <= M; ++j)
            {
                if (cluster[j] == cluster[i])
                {                           // 如果标签j和标签i属于同一簇
                    new_group.push_back(j); // 将标签j加入新的簇
                    assigned[j] = group_id; // 记录标签j的聚类结果
                }
            }
            grouped_labels.push_back(new_group); // 将新的簇加入分组结果
            ++group_id;                          // 增加聚类id
        }
    }

    // 5. 生成全局排序
    position = generateGlobalOrder(grouped_labels, frequent, max); // 用1-16

    // 初始化磁盘空间
    init_disk_space();

    // 将disk数组中的所有元素初始化为0
    for (int i = 0; i < MAX_DISK_NUM; i++)
    {
        fill(disk[i].begin(), disk[i].end(), 0);
    }

    // 初始化空闲区间
    init_free_segments();

    // 初始化磁盘状态
    for (int i = 1; i <= N; i++)
    {
        disk_states[i] = DiskState();
        disk_states[i].tokens_left = G;
    }
}

int main()
{
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    // T代表本次数据有T+105个时间片 第T+1到T+105个时间片中。没有删除、写入、读取请求
    // M代表对象标签数 1<=M<=16
    // N代表系统中硬盘/磁头的个数
    // V代表硬盘中的存储单元个数
    // G代表每个磁头每个时间片最多消耗的令牌数

    int time = (T + 105) % FRE_PER_SLICING ? ((T + 105) / FRE_PER_SLICING + 1) : ((T + 105) / FRE_PER_SLICING); // 有多少个 1800时间片

    vector<vector<int>> delete_num(M + 1, vector<int>(time + 1, 0));
    vector<vector<int>> write_num(M + 1, vector<int>(time + 1, 0));
    vector<vector<int>> read_num(M + 1, vector<int>(time + 1, 0));
    // vector<int> max_delete(M + 1);
    // vector<int> max_write(M + 1);
    vector<int> max(M + 1, 0);

    // 二维数组，第一维是各个标签，第二维是各个 1800时间片 内相应操作的对象个数

    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            // 处理第j个1800时间片中，要删除的标签为i的对象的大小之和（总共多少个对象快）
            scanf("%d", &delete_num[i][j]);
        }
    }

    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            // 处理第j个1800时间片中，要写入的标签为i的对象的大小之和
            scanf("%d", &write_num[i][j]);
        }
    }

    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            // 计算写入与删除操作频率差值
            delete_num[i][j] = write_num[i][j] - delete_num[i][j];
            if (write_num[i][j] > max[i])
            {
                max[i] = delete_num[i][j];
            }
        }
    }

    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            // 处理第j个1800时间片中，要读取的标签为i的对象的大小之和
            scanf("%d", &read_num[i][j]);
        }
    }

    // 初始化系统
    Init(read_num, N, max);

    printf("OK\n");
    fflush(stdout);

    // 初始化硬盘磁头的位置
    for (int i = 1; i <= N; i++)
    {
        disk_point[i] = 1;
    }

    for (int t = 1; t <= T + EXTRA_TIME; t++)
    {
        // 处理第i个时间片的操作
        timestamp_action(); // 时间戳事件交互
        delete_action();    // 删除事件交互
        write_action();     // 写入事件交互
        read_action();      // 读取事件交互
    }
    clean();

    return 0;
}