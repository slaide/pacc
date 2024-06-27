struct Person{
	int age;
	char* name;
};
typedef struct Person Person;
int func(int a,char* b){
	return 0;
}
bool test1(){
	int sizeofperson=func(sizeof(struct Person),&(Person){});

	return false;
}
