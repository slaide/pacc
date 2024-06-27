int main(){
    int a=(2);
    int b=((3));
}
#define B {}
#define F(A) A
#define F2(A) F (A) B
F2(int main2())
