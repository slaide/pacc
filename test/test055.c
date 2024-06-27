#define USING_FEATURE_A848V 1
#define USING_FEATURE(F) USING_FEATURE_##F
#if USING_FEATURE(A848V)
#define main main
int main(){
#endif
    return 0;
}
