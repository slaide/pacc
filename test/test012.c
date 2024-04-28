#include<stdio.h>
#include<stdlib.h>
#include<string.h>

struct Person{
	int age;
	char*name;
	int num_children;
	struct Person*children;
};
void Person_print(struct Person*person){
	printf("Person(%s,%d)\n",person->name,person->age);
	for(int i=0;i<person->num_children;i++){
		printf("%ss child %d: ",person->name,i);
		Person_print(&person->children[i]);
	}
}

void*allocAndCopy(int size,void*src){
	char*dest=malloc(size);
	memcpy(dest,src,size);
	return dest;
}

bool test1(){
	struct Person* peter=allocAndCopy(sizeof(struct Person),&(struct Person){
		.name="Peter",
		.age=2,
		.num_children=2,
		.children=allocAndCopy(2*sizeof(struct Person),&(struct Person[2]){
			[0]=(struct Person){
				.name="Paul",
				.age=3,
				.num_children=0,
				.children=nullptr,
			},
			[1]=(struct Person){
				.age=4,
				.num_children=0,
				.children=nullptr,
			},
			[1].name="Mary",
		}),
	});

	if(peter->num_children!=2)
		return false;

	return false;
}
