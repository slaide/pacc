struct Person{char*name;};
int main(){
    int a=sizeof(int);
    int b=sizeof(a);
    int c=sizeof(struct Person);
    struct Person peter;
    int d=sizeof(peter);
    return 0;
}
