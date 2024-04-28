#include<stdlib.h>
#include<stdio.h>
struct Person{
    int age;
    char*name;
};
char* Person_asString(struct Person* person){
	char*ret=calloc(1024,1);
    sprintf(ret,"%s %s is %d years old ",(person->age>=18)?"Adult":"Child",person->name,person->age);
    return ret;
}
