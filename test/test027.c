struct Person;
union Data;
struct MyData;

enum JOB{
    MANAGER,
    ENGINEER=1,
    CEO,
    JANITOR,
};

struct Person{
    char*name;
    int age;
    float height_in_m;

    enum JOB job;

    struct Person* parents[2];
};

union Data{
    int i;
    float f;
    char c;
};
struct MyData{
    union Data data;
    struct MyData* next;
};
int;
struct Test{
    union{
        int i;
        float f;
    };
    int;
};

// TESTGOAL unnamed symbol declarations, type declarations, type definitions
