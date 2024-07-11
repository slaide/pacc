void func(int a){
    return;
}
struct Person{
    char*name;
};
int main(){
    func((struct Person){.name="peter"});
}
