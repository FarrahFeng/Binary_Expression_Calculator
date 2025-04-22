#include <stdio.h>
#include <string.h>
#include <ctype.h>

int main(void){
    int x=2,y=9,z=6;
    //int x=11,y=9,z=6;
    char c[20];
    //printf("x=%d,y=%d,z=%d",x,y,z);
    fgets(c,20,stdin);
    int cLen=strlen(c);
    for (int i=0;i<cLen;i++){
        printf("%c\n",c[i]);
        if(c[i]=='\0')
            printf("enter i=%d\n",i);
    }
    

   
}