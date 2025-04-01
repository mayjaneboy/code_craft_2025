#include <cstdio>
#include <cassert> //assert()
#include <cstdlib>

#define MAX_DISK_NUM (10 + 1)//�洢ϵͳ��Ӳ�̵ĸ������ֵ
#define MAX_DISK_SIZE (16384 + 1)//Ӳ�̵Ĵ洢��Ԫ���������ֵ
#define MAX_REQUEST_NUM (30000000 + 1)//ĳһʱ��Ƭ�ڶ�ȡ������ܴ��������ֵ
#define MAX_OBJECT_NUM (100000 + 1)//ĳһʱ��Ƭ��ɾ����д�������ܴ�������֤С�ڵ���100000
#define REP_NUM (3)//��������
#define FRE_PER_SLICING (1800)
#define EXTRA_TIME (105)
//������105��ʱ��Ƭ�ڣ��������ݱ�֤û��ɾ����д��Ͷ�ȡ����

typedef struct Request_ {
    int object_id;//�ö�ȡ�����ȡ�Ķ���ı��
    int prev_id;//�ö�ȡ�����ȡ�Ķ������һ�ζ�ȡ������
    bool is_done;//��ȡ�����Ƿ����
} Request;//��ȡ��������

typedef struct Object_ {
    int replica[REP_NUM + 1];//�ö���Ĵ洢�����Ĵ��̱������
    int* unit[REP_NUM + 1];//�ö����ڴ洢���������ϵĴ洢��Ԫ�ı������ָ�룬
    //ÿ������Ԫ�ؿ�������һ�����������,ָ��ö�����һ���洢���������ϵĶ���洢��Ԫ�ı��
    int size;//�����С
    int last_request_point;//���һ�ζ�ȡ����
    bool is_delete;//�Ƿ�ɾ��
} Object;

Request request[MAX_REQUEST_NUM];//��ȡ��������
Object object[MAX_OBJECT_NUM];//��������

int T, M, N, V, G;
// T������������T+105��ʱ��Ƭ ��T+1��T+105��ʱ��Ƭ�С�û��ɾ����д�롢��ȡ����
// M��������ǩ�� 1<=M<=16 N����ϵͳ��Ӳ�̵ĸ��� V����Ӳ���еĴ洢��Ԫ����
// G����ÿ����ͷÿ��ʱ��Ƭ������ĵ�������


int disk[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_point[MAX_DISK_NUM];

void timestamp_action()
{
    int timestamp;
    scanf("%*s%d", &timestamp);//%*s��ȡһ���ַ�������������
    //�����Ƕ�ȡ���������������� TIMESTAMP current_timestamp�е�TIMESTAMP
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
}

void do_object_delete(const int* object_unit, int* disk_unit, int size)
{
    //ɾ���ڸø��������ϣ��ö���Ĳ�ͬ��
    for (int i = 1; i <= size; i++) {
        disk_unit[object_unit[i]] = 0;
        //object_unit�����ڸø��������ϣ��ö���Ĳ�ͬ��Ĵ洢��Ԫ�������
        //disk_unit������Ϊid�Ķ���ĵ�j���������������д��������еı��
    }
}
//ɾ���¼�����
void delete_action()
{
    int n_delete;//��һʱ��Ƭ�ڱ�ɾ���Ķ������
    int abort_num = 0;//��ʱ��Ƭ�ڱ�ȡ���Ķ�ȡ���������
    static int _id[MAX_OBJECT_NUM];//Ҫ��ɾ���Ķ����id����

    scanf("%d", &n_delete);
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]);
    }//����������

    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point;//��ȡ�ö�������һ��������
        while (current_id != 0) {
            if (request[current_id].is_done == false) {//���������û�����
                abort_num++;//��ֹͣ�Ķ�ȡ����������һ
            }
            current_id = request[current_id].prev_id;//��ȡ�ö������һ����ȡ������
        }
    }

    printf("%d\n", abort_num);//�����ֹͣ�Ķ�ȡ��������
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];//����ÿһ��Ҫ��ɾ���Ķ��󣬻�ȡ���ţ�����n_delete<=����_id[]�ĳ���
        int current_id = object[id].last_request_point;//��ȡ�ö�������һ�ζ�ȡ������
        while (current_id != 0) {//�����ö�������ж�ȡ���󣨲�ֻ������һ��ʱ��Ƭ�ڣ�
            if (request[current_id].is_done == false) {
                printf("%d\n", current_id);//���û����ɣ������
            }
            current_id = request[current_id].prev_id;//��ȡ�ö�ȡ�����ȡ�Ķ������һ�ζ�ȡ������
        }
        for (int j = 1; j <= REP_NUM; j++) {//����ɾ���ö�������и���
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
            //object[id].unit[j] ���Ϊid�Ķ���ĸ����������ĵ�j���洢�����ϵĴ洢��Ԫ�������
            //disk[object[id].replica[j]] ���Ϊid�Ķ���ĵ�j���������������д��������еı��
        }
        object[id].is_delete = true;
    }

    fflush(stdout);//ǿ��ˢ�±�׼�����������ȷ������������������ܻ������Ӱ��
}

void do_object_write(int* object_unit, int* disk_unit, int size, int object_id)
{

    int current_write_point = 0;//��¼�Ѿ������˶��ٸ��洢��Ԫ
    for (int i = 1; i <= V; i++) {
        //����Ӳ���е�ÿһ���洢��Ԫ
        if (disk_unit[i] == 0) {

            //�����ǰ�洢��ԪΪ�գ��ǾͰ�����洢��Ԫ��ֵΪ�ö����id����ʾ���ö���ռ��
            disk_unit[i] = object_id;
            //�� ����洢��Ԫ�ı�� �ŵ� �������������洢����Ӳ���еĴ洢��Ԫ������� ��
            object_unit[++current_write_point] = i;

            //���������õ��� ˳���ҿ��еĴ洢��Ԫ �Ĳ���

            if (current_write_point == size) {break;}
            //ÿһ������춼�ѱ������˴洢��Ԫ���˳�ѭ��
        }
    }

    assert(current_write_point == size);
    //ȷ����������п鶼�������˴洢��Ԫ
    //assert(����) ����Ϊtrue�������ִ�У��������ֹ������
}
//д���¼�����
void write_action()
{
    int n_write;//��һʱ��Ƭ��Ҫд��Ķ���ĸ���
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++) {
        //n_write��Ϊ0��Ϊ0���������κ����
        //����ÿһ��Ҫд��Ķ���
        int id, size;
        scanf("%d%d%*d", &id, &size);//��ȡ�������ı�źʹ�С
        object[id].last_request_point = 0;//�����һ�ζ�ȡ������Ϊ0

        for (int j = 1; j <= REP_NUM; j++) {//�洢������Ӳ����
            //��������object[MAX_OBJECT_NUM]�е�id��Ԫ�ؾ��Ǹö���
            // 
            //�����������Ĵ洢�����Ĵ��̱�����飬Ҳ���ǰѸö���洢���Ǽ���Ӳ����
            object[id].replica[j] = (id + j) % N + 1;
            //�������Ĳ���������ѭ���浽ÿһ��Ӳ����

            //�� ��������ڵ�j���洢����Ӳ���д洢��Ԫ�ı������ ��̬�����ڴ�
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1)));
            //malloc(sizeof(int) * (size + 1))����size+1��Int���͵��ڴ�ռ䣬������void*
            //����static_cast<int*>ǿ������ת��ΪInt*

            object[id].size = size;//���ö���Ĵ�С
            object[id].is_delete = false;//���ö����״̬

            do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id);
            //�� ��������ڵ�j���洢����Ӳ���д洢��Ԫ�ı�����飬
            //   �������ĵ�J���洢����Ӳ�̵Ĵ洢��Ԫ���飨����һ�����飬��Ϊdisk[]�Ƕ�ά���飬��ڶ�ά����һ��Ӳ�̵Ĵ洢��Ԫ��
            //   ���ж���Ĵ�С����� ��Ϊʵ�δ���do_object_write()
        }
        //�ö�������д洢�������������

        printf("%d\n", id);//����������ı��
        for (int j = 1; j <= REP_NUM; j++) {
            //����ÿһ������
            printf("%d", object[id].replica[j]);//����������Ӳ�̵ı��
            for (int k = 1; k <= size; k++) {
                //������������Ӳ���У������Ĵ洢��Ԫ���
                printf(" %d", object[id].unit[j][k]);
            }
            printf("\n");
        }
    }

    //�����д�����˳����������뱣��һ�£�������Ҫ�ڸ�ʱ��Ƭ�������е�д������
    //�������Ƕ�ÿһ��Ҫд��������д�����֮����ģ������Ǳ���һ�µ�

    fflush(stdout);
}
//��ȡ�¼�����
void read_action()
{
    int n_read;//��һʱ��Ƭ��ȡ����ĸ���
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++) {
        //����ÿһ��Ҫ��ȡ�Ķ���
        //��ȡ�����ź�����Ķ�����
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;//���ø���������������
        request[request_id].prev_id = object[object_id].last_request_point;//��¼ �ö�ȡ�����ȡ�Ķ������һ�ζ�ȡ������
        object[object_id].last_request_point = request_id;//����������һ�ζ�ȡ��������Ϊ��ǰ��ȡ����ı��
        request[request_id].is_done = false;//�ö�ȡ����δ���
    }
    //��ȡ���������ʼ�����

    static int current_request = 0;//��¼��ǰ���ڴ����������
    static int current_phase = 0;//��¼��ǰ����Ľ׶���
    //current_request��current_phase��static���Σ��⼴�����Ĵ洢λ���ھ�̬��ȫ�֣��洢����������ͨ�ֲ������洢��ջ��
    //������������read_action()���غ󣬱�����Ȼ���ڣ������ں������ý���ʱ������
    //��������������Ȼ��read_action()�ڲ����ⲿ�����޷�ֱ�ӷ�����
    //�����������ǿ�ʱ��Ƭ�ģ���һ��read_action()���ö�Ӧһ��ʱ��Ƭ��������Ҫ�纯�����ñ�����ֵ

    if (!current_request && n_read > 0) {
        //ֻ�������ʱ��Ƭ�ж�����n_read > 0������ û��֮ǰʱ��Ƭ�Ķ���������ִ�У�current_requestΪ0������Ҫ�ѵ�ǰʱ��Ƭ�Ķ���������Ϊ��ǰ����ִ�еĶ�����
        current_request = request_id;//��ʼ��current_request Ϊ��ǰ request_id
        //request_id ����forѭ����ָ��������һ����ȡ����ı�ţ���������current_request
        //�Ǿ��Ǵ����ʱ��Ƭ�����һ��������ʼ������Ϊʲô���ӵ�һ����ʼ��
    }
    if (!current_request) {
        //�� current_request ��ȻΪ��
        for (int i = 1; i <= N; i++) {
            printf("#\n");//��ͷ���˶����Ϊ"#"
        }
        printf("0\n");//���0��n_rsp���Ǹ�ʱ��Ƭ���ϱ��ɹ��Ķ�ȡ������Ŀ
    } else {
        //��Ϊ��
        current_phase++;//�׶�������
        object_id = request[current_request].object_id;//��ȡ��ǰ����Ķ����id
        for (int i = 1; i <= N; i++) {
            //����Ӳ��
            if (i == object[object_id].replica[1]) {
                //���i==�������ĵ�һ���洢����Ӳ�̵ı�ţ�Ҳ�����ڵ�һ���洢����Ӳ����
                if (current_phase % 2 == 1) {
                    //�����׶�
                    printf("j %d\n", object[object_id].unit[1][current_phase / 2 + 1]);
                    //object[object_id].unit[1][current_phase / 2 + 1]
                    // ��ʾ�������ĵ�һ���洢����Ӳ�̵ĵ�current_phase / 2 + 1���洢��Ԫ���
                    //ִ��Jump��������� j ��Ծ���Ĵ洢��Ԫ���
                    //ֻ����ÿ��ʱ��Ƭ�Ŀ�ʼִ��"Jump"��������ִ�к�ô�ͷ������������������
                } else {
                    //ż���׶�
                    printf("r#\n");
                    //ִ��read����
                }
            } else {
                //��������������ĵ�һ���洢����Ӳ�̵ı�ţ��ͽ�����
                // Ϊʲô���������ĸ���Ӳ���ж�ȡ
                printf("#\n");
                //��ͷ�˶�����
            }
        }

        if (current_phase == object[object_id].size * 2) {
            //����׶������ڶ�ȡ����Ĵ�С������
            if (object[object_id].is_delete) {
                //����������ɾ����
                printf("0\n");//���0����n_rsp��ǰʱ��Ƭ�϶�ȡ��ɵ������������ȷʵ�����������ʱ��Ƭ�ϱ���ȡ��ɵĶ�ȡ������
            } else {
                //���ǣ�����������û��ɾ��
                printf("1\n%d\n", current_request);
                //���1����ʱ��Ƭֻ��һ����ȡ��ɵ�����
                //current_request�������ɵĶ�ȡ������
                //�ǲ�����һ��ʱ��Ƭ��ֻ����һ����ȡ������
                request[current_request].is_done = true;
            }
            //ʱ��Ƭ��������ǰ�Ķ�ȡ��������Ϊ0���׶�����Ϊ0
            current_request = 0;
            current_phase = 0;
        } else {
            //Ϊʲô�����ڵ�ʱ�򣬾���һ����û�ж�ȡ���
            printf("0\n");
        }
    }

    fflush(stdout);
}

void clean()
{
    for (auto& obj : object) {
        //����ÿһ������
        for (int i = 1; i <= REP_NUM; i++) {
            //�ö����ÿһ������
            if (obj.unit[i] == nullptr)
                continue;
            //��� �ö���������һ�������Ĵ洢��Ԫ����ָ�� ��Ϊ��
            free(obj.unit[i]);
            //�ͷŸ�ָ��ָ�ŵ�������ռ���ڴ�ռ�
            obj.unit[i] = nullptr;
            //�ø�ָ��Ϊ��
        }
    }
}

int main()
{
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    // T������������T+105��ʱ��Ƭ ��T+1��T+105��ʱ��Ƭ�С�û��ɾ����д�롢��ȡ����
    // M��������ǩ�� 1<=M<=16
    // N����ϵͳ��Ӳ�̵ĸ���
    // V����Ӳ���еĴ洢��Ԫ����
    // G����ÿ����ͷÿ��ʱ��Ƭ������ĵ�������

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            //�����j��1800ʱ��Ƭ�У�Ҫɾ���ı�ǩΪi�Ķ���Ĵ�С֮�ͣ��ܹ����ٸ�����죩
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            //�����j��1800ʱ��Ƭ�У�Ҫд��ı�ǩΪi�Ķ���Ĵ�С֮��
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            //�����j��1800ʱ��Ƭ�У�Ҫ��ȡ�ı�ǩΪi�Ķ���Ĵ�С֮��
            scanf("%*d");
        }
    }

    printf("OK\n");
    fflush(stdout);

    //��ʼ��Ӳ������
    for (int i = 1; i <= N; i++) {
        disk_point[i] = 1;
    }

    
    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        //�����i��ʱ��Ƭ�Ĳ���
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }
    clean();

    return 0;
}