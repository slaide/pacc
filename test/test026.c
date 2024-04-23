#include<stdio.h>
int main(){
    while(1){
        break;
    }
    for(int i=0;;i++){
        if(i==10){
            continue;
        }
        printf("i is %d",i);
        if(i>0 && i%111==0){
            break;
        }
    }

    return 0;
}

// TESTGOAL break and continue in loops, modulo operator
